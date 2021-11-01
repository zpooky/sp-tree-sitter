#define _GNU_SOURCE
#include <tree_sitter/api.h>

#include "shared.h"
#include "sp_str.h"

#include <string.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <stdint.h>
#include <assert.h>

enum sp_ts_SourceDomain {
  DEFAULT_DOMAIN = 0,
  SYSLOG_DOMAIN,
  LINUX_KERNEL_DOMAIN,
};

struct sp_ts_Context {
  struct sp_ts_file file;
  TSTree *tree;
  enum sp_ts_SourceDomain domain;
};

extern const TSLanguage *
tree_sitter_c(void);

extern const TSLanguage *
tree_sitter_cpp(void);

struct arg_list;
struct arg_list {
  char *format;
  char *variable;
  bool complete;
  char *complex_raw;
  char *type;
  bool complex_printf;
  uint32_t pointer;

  bool function_pointer;

  bool dead;

  bool is_array;
  char *variable_array_length;

  struct arg_list *rec;

  struct arg_list *next;
};

static struct arg_list *
__field_to_arg(struct sp_ts_Context *ctx, TSNode subject, const char *pprefix);

static void
debug_subtypes_rec(struct sp_ts_Context *ctx, TSNode node, size_t indent)
{
  uint32_t i;
  for (i = 0; i < ts_node_child_count(node); ++i) {
    size_t a;
    TSNode child     = ts_node_child(node, i);
    const char *type = ts_node_type(child);
    for (a = 0; a < indent; ++a) {
      fprintf(stderr, "  ");
    }
    fprintf(stderr, "[%s]", type);
    if (strcmp(type, "field_identifier") == 0 ||
        strcmp(type, "primitive_type") == 0 ||
        strcmp(type, "number_literal") == 0 ||
        strcmp(type, "identifier") == 0 ||
        strcmp(type, "type_identifier") == 0) {
      uint32_t s   = ts_node_start_byte(child);
      uint32_t e   = ts_node_end_byte(child);
      uint32_t len = e - s;
      fprintf(stderr, ": %.*s", (int)len, &ctx->file.content[s]);
    }
    fprintf(stderr, "\n");

    debug_subtypes_rec(ctx, child, indent + 1);
  }
}
struct list_TSNode;
struct list_TSNode {
  TSNode node;
  struct list_TSNode *next;
};
static struct list_TSNode *
__leafs(struct sp_ts_Context *ctx, TSNode subject, struct list_TSNode *result)
{
  uint32_t i;
  for (i = 0; i < ts_node_child_count(subject); ++i) {
    TSNode child = ts_node_child(subject, i);
    if (ts_node_child_count(child) > 0) {
      struct list_TSNode *tmp;
      if ((tmp = __leafs(ctx, child, result))) {
        result = tmp;
      }
    } else {
      result = result->next = calloc(1, sizeof(*result));
      result->node          = child;
    }
  }
  return result;
}

static bool
is_c_file(const char *file)
{
  struct sp_str str;
  bool result;
  sp_str_init_cstr(&str, file);
  result =
    sp_str_postfix_cmp(&str, ".c") == 0 || sp_str_postfix_cmp(&str, ".h") == 0;
  sp_str_free(&str);
  return result;
}

static bool
is_cpp_file(const char *file)
{
  struct sp_str str;
  bool result;
  sp_str_init_cstr(&str, file);
  result = sp_str_postfix_cmp(&str, ".cc") == 0 ||
           sp_str_postfix_cmp(&str, ".cpp") == 0 ||
           sp_str_postfix_cmp(&str, ".hpp") == 0 ||
           sp_str_postfix_cmp(&str, ".hh") == 0;
  sp_str_free(&str);
  return result;
}

static enum sp_ts_SourceDomain
get_domain(const char *file)
{
  return strcasestr(file, "/linux-axis") != NULL
           ? LINUX_KERNEL_DOMAIN
           : strcasestr(file, "/dists/") != NULL ? SYSLOG_DOMAIN
                                                 : DEFAULT_DOMAIN;
}

static TSNode
sp_find_parent(TSNode subject,
               const char *needle0,
               const char *needle1,
               const char *needle2,
               const char *needle3)
{
  TSNode it     = subject;
  TSNode result = {0};

  while (!ts_node_is_null(it)) {
    if (strcmp(ts_node_type(it), needle0) == 0 ||
        strcmp(ts_node_type(it), needle1) == 0 ||
        strcmp(ts_node_type(it), needle2) == 0 ||
        strcmp(ts_node_type(it), needle3) == 0) {
      result = it;
    }
    it = ts_node_parent(it);
  }
  return result;
}

static TSNode
find_direct_chld_by_type(TSNode subject, const char *needle)
{
  TSNode empty = {0};
  uint32_t i;
  for (i = 0; i < ts_node_child_count(subject); ++i) {
    TSNode child          = ts_node_child(subject, i);
    const char *node_type = ts_node_type(child);
    if (strcmp(node_type, needle) == 0) {
      return child;
    }
  }
  return empty;
}

static char *
sp_struct_value(struct sp_ts_Context *ctx, TSNode subject)
{
  uint32_t s   = ts_node_start_byte(subject);
  uint32_t e   = ts_node_end_byte(subject);
  uint32_t len = e - s;
  assert(e >= s);
  if (len == 0) {
    return NULL;
  }
  return strndup(&ctx->file.content[s], len);
}

struct sp_str_list;
struct sp_str_list {
  char *value;
  struct sp_str_list *next;
};

static int
sp_print_enum(struct sp_ts_Context *ctx, TSNode subject)
{
  int res = EXIT_SUCCESS;
  TSNode tmp;
  sp_str buf;
  char *type_name              = NULL;
  bool type_name_t             = false;
  struct sp_str_list dummy     = {0};
  struct sp_str_list *enums_it = &dummy;
  bool enum_class              = false;

  sp_str_init(&buf, 0);

  /* fprintf(stderr, "%s\n", __func__); */

  tmp        = find_direct_chld_by_type(subject, "class");
  enum_class = !ts_node_is_null(tmp);

  tmp = find_direct_chld_by_type(subject, "type_identifier");
  if (!ts_node_is_null(tmp)) {
    /* struct type_name { ... }; */
    type_name = sp_struct_value(ctx, tmp);
  } else {
    TSNode parent = ts_node_parent(subject);
    if (!ts_node_is_null(parent)) {
      if (strcmp(ts_node_type(parent), "type_definition") == 0) {
        tmp = find_direct_chld_by_type(parent, "type_identifier");
        if (!ts_node_is_null(tmp)) {
          /* typedef struct * { ... } type_name; */
          type_name_t = true;
          type_name   = sp_struct_value(ctx, tmp);
        }
      }
    }
  }

  tmp = find_direct_chld_by_type(subject, "enumerator_list");
  if (!ts_node_is_null(tmp)) {
    uint32_t i;
    for (i = 0; i < ts_node_child_count(tmp); ++i) {
      TSNode enumerator = ts_node_child(tmp, i);
      if (strcmp(ts_node_type(enumerator), "enumerator") == 0) {
        if (ts_node_child_count(enumerator) > 0) {
          struct sp_str_list *arg = NULL;
          if ((arg = calloc(1, sizeof(*arg)))) {
            TSNode id  = ts_node_child(enumerator, 0);
            arg->value = sp_struct_value(ctx, id);
            enums_it = enums_it->next = arg;
          }
        }
#if 0
        uint32_t a;
        for (a = 0; a < ts_node_child_count(enumerator); ++a) {
          TSNode xx = ts_node_child(enumerator, a);
          fprintf(stderr, "a.%u\n", a);
          fprintf(stderr, "%s\n", ts_node_type(xx));
          fprintf(stderr, "%s\n", sp_struct_value(ctx, xx));
        }
#endif
      }
    } //for
  }

  if (!type_name) {
    res = EXIT_FAILURE;
    goto Lout;
  }

  sp_str_append(&buf, "static inline const char* sp_debug_");
  sp_str_append(&buf, type_name);
  sp_str_append(&buf, "(const ");
  sp_str_append(&buf, type_name_t ? "" : "enum ");
  sp_str_append(&buf, type_name);
  sp_str_append(&buf, " *in) {\n");
  sp_str_append(&buf, "  if (!in) return \"NULL\";\n");
  sp_str_append(&buf, "  switch (*in) {\n");
  enums_it = dummy.next;
  while (enums_it) {
    sp_str_append(&buf, "    case ");
    if (enum_class) {
      sp_str_appends(&buf, type_name, "::", NULL);
    }
    sp_str_append(&buf, enums_it->value);
    sp_str_append(&buf, ": return \"");
    sp_str_append(&buf, enums_it->value);
    sp_str_append(&buf, "\";\n");
    enums_it = enums_it->next;
  }

  sp_str_append(&buf, "    default: return \"__UNDEF\";\n");
  sp_str_append(&buf, "  }\n");
  sp_str_append(&buf, "}\n");

  fprintf(stdout, "%s", sp_str_c_str(&buf));

Lout:
  sp_str_free(&buf);
  free(type_name);
  enums_it = dummy.next;
  while (enums_it) {
    struct sp_str_list *next = enums_it->next;
    free(enums_it->value);
    free(enums_it);
    enums_it = next;
  }
  return res;
}

static TSNode
__rec_search(struct sp_ts_Context *ctx,
             TSNode subject,
             const char *needle,
             uint32_t curlevel,
             uint32_t *level)
{
  TSNode empty = {0};
  uint32_t i;
  for (i = 0; i < ts_node_child_count(subject); ++i) {
    TSNode tmp;
    TSNode child = ts_node_child(subject, i);

    if (strcmp(ts_node_type(child), needle) == 0) {
      assert(ts_node_child_count(child) == 0);
      *level = curlevel;
      return child;
    }
    tmp = __rec_search(ctx, child, needle, curlevel + 1, level);
    if (!ts_node_is_null(tmp)) {
      return tmp;
    }
  }
  return empty;
}

static char *
__field_type(struct sp_ts_Context *ctx,
             TSNode subject,
             struct arg_list *result,
             const char *pprefix)
{
  TSNode tmp;
  char *type = NULL;

  tmp = find_direct_chld_by_type(subject, "primitive_type");
  if (!ts_node_is_null(tmp)) {
    fprintf(stderr, "1\n");
    /* $primitive_type $field_identifier; */
    type = sp_struct_value(ctx, tmp);
  } else {
    tmp = find_direct_chld_by_type(subject, "sized_type_specifier");
    if (!ts_node_is_null(tmp)) {
      fprintf(stderr, "2\n");
      sp_str tmp_str;
      uint32_t i;

      sp_str_init(&tmp_str, 0);
      for (i = 0; i < ts_node_child_count(tmp); ++i) {
        TSNode child   = ts_node_child(tmp, i);
        char *tmp_type = sp_struct_value(ctx, child);
        if (tmp_type) {
          if (!sp_str_is_empty(&tmp_str)) {
            sp_str_append(&tmp_str, " ");
          }
          sp_str_append(&tmp_str, tmp_type);
        }
        free(tmp_type);
      } //for

      /* $sized_type_specifier $sized_type_specifier ... $field_identifier; */
      type = strdup(sp_str_c_str(&tmp_str));
      sp_str_free(&tmp_str);
    } else {
      tmp = find_direct_chld_by_type(subject, "type_identifier");
      if (!ts_node_is_null(tmp)) {
        /* fprintf(stderr, "3\n"); */
        /* $type_identifier $field_identifier;
         * Example:
         *  type_t type0;
         *  gint int0;
         */
        type = sp_struct_value(ctx, tmp);
      } else {
        tmp = find_direct_chld_by_type(subject, "enum_specifier");
        if (!ts_node_is_null(tmp)) {
          TSNode type_id;

          type_id = find_direct_chld_by_type(tmp, "type_identifier");
          if (!ts_node_is_null(type_id)) {
            type = sp_struct_value(ctx, type_id);
          } else {
            TSNode enum_list;

            enum_list = find_direct_chld_by_type(tmp, "enumerator_list");
            if (!ts_node_is_null(enum_list)) {
              struct sp_str_list enum_dummy = {0};
              struct sp_str_list *enums_it  = &enum_dummy;
              sp_str buf_tmp;
              uint32_t i;

              sp_str_init(&buf_tmp, 0);

              for (i = 0; i < ts_node_child_count(enum_list); ++i) {
                TSNode enumerator = ts_node_child(enum_list, i);
                if (strcmp(ts_node_type(enumerator), "enumerator") == 0) {
                  uint32_t a;

                  for (a = 0; a < ts_node_child_count(enumerator); ++a) {
                    TSNode enum_id = ts_node_child(enumerator, a);
                    if (strcmp(ts_node_type(enum_id), "identifier") == 0) {
                      enums_it = enums_it->next = calloc(1, sizeof(*enums_it));
                      enums_it->value           = sp_struct_value(ctx, enum_id);
                    }
                  } //for
                }
              } //for
#if 0
              fprintf(stderr, "------------enum\n");
              for (i = 0; i < ts_node_child_count(enum_list); ++i) {
                TSNode child = ts_node_child(enum_list, i);
                uint32_t s   = ts_node_start_byte(child);
                uint32_t e   = ts_node_end_byte(child);
                uint32_t len = e - s;
                fprintf(stderr, ".%u\n", i);
                fprintf(stderr, "children: %u\n", ts_node_child_count(child));
                fprintf(stderr, "%.*s: %s\n", (int)len, &ctx->file.content[s],
                        ts_node_type(child));
              }
              fprintf(stderr, "------------enum END\n");
#endif
              enums_it = enum_dummy.next;
              while (enums_it) {
                sp_str_appends(&buf_tmp, pprefix, result->variable,
                               " == ", enums_it->value, " ? \"",
                               enums_it->value, "\" : ", NULL);
                enums_it = enums_it->next;
              }
              if (enum_dummy.next) {
                sp_str_append(&buf_tmp, "\"__UNDEF\"");
                /* enum { ONE, ... } field_identifier; */
                result->format         = "%s";
                result->complex_raw    = strdup(sp_str_c_str(&buf_tmp));
                result->complex_printf = true;
              }

              enums_it = enum_dummy.next;
              while (enums_it) {
                struct sp_str_list *next = enums_it->next;
                free(enums_it);
                enums_it = next;
              }
              sp_str_free(&buf_tmp);
            }
          }
        } else {
          TSNode struct_spec;
          struct_spec = find_direct_chld_by_type(subject, "struct_specifier");
          if (!ts_node_is_null(struct_spec)) {
            TSNode type_id;
            fprintf(stderr, "5\n");
            /* debug_subtypes_rec(ctx, subject, 0); */
            type_id = find_direct_chld_by_type(struct_spec, "type_identifier");
            if (!ts_node_is_null(type_id)) {
              fprintf(stderr, "5.1\n");
              type = sp_struct_value(ctx, type_id);
            } else {
              TSNode field_decl_l;
              fprintf(stderr, "5.2\n");
              field_decl_l =
                find_direct_chld_by_type(struct_spec, "field_declaration_list");
              if (!ts_node_is_null(field_decl_l)) {
                uint32_t i;
                struct arg_list field_dummy = {0};
                struct arg_list *field_it   = &field_dummy;
                fprintf(stderr, "5.2.1\n");
                for (i = 0; i < ts_node_child_count(field_decl_l); ++i) {
                  TSNode field = ts_node_child(field_decl_l, i);
                  /* fprintf(stderr, "i.%u\n", i); */
                  if (strcmp(ts_node_type(field), "field_declaration") == 0) {
                    struct arg_list *arg = NULL;

                    if ((arg = __field_to_arg(ctx, field, "in->"))) {
                      field_it->next = arg;
                      while (field_it->next) {
                        field_it = field_it->next;
                      }
                    }
                  }
                } //for
                result->rec = field_dummy.next;

#if 0
                {
                  unsigned i;
                  for (i = 0; i < ts_node_child_count(field_id); ++i) {
                    TSNode child = ts_node_child(field_id, i);
                    uint32_t s   = ts_node_start_byte(child);
                    uint32_t e   = ts_node_end_byte(child);
                    uint32_t len = e - s;
                    fprintf(stderr, ".%u\n", i);
                    fprintf(stderr, "children: %u\n",
                            ts_node_child_count(child));
                    fprintf(stderr, "%.*s: %s\n", (int)len,
                            &ctx->file.content[s], ts_node_type(child));
                  }
                }
                uint32_t s   = ts_node_start_byte(field_decl_l);
                uint32_t e   = ts_node_end_byte(field_decl_l);
                uint32_t len = e - s;
                fprintf(stderr, "%.*s: %s\n", (int)len, &ctx->file.content[s],
                        ts_node_type(field_decl_l));
#endif
              } else {
                fprintf(stderr, "5.2.2\n");
              }
            }
          }
        }
      }
    }
  }
  return type;
}

static struct arg_list *
__field_name(struct sp_ts_Context *ctx, TSNode subject, const char *identifier)
{
  struct arg_list *result = NULL;
  TSNode tmp;
  result = calloc(1, sizeof(*result));

  /* debug_subtypes_rec(ctx, subject, 0); */
  tmp = find_direct_chld_by_type(subject, identifier);
  if (!ts_node_is_null(tmp)) {
    result->variable = sp_struct_value(ctx, tmp);
    /* fprintf(stderr, "1: %s\n", result->variable); */
  } else {
    tmp = find_direct_chld_by_type(subject, "pointer_declarator");
    if (!ts_node_is_null(tmp)) {
      tmp = __rec_search(ctx, tmp, identifier, 1, &result->pointer);
      if (!ts_node_is_null(tmp)) {
        result->variable = sp_struct_value(ctx, tmp);
      }
    } else {
      tmp = find_direct_chld_by_type(subject, "array_declarator");
      if (!ts_node_is_null(tmp)) {
        uint32_t i;
        TSNode field_id;
        bool start_found = false;

        for (i = 0; i < ts_node_child_count(tmp); ++i) {
          TSNode child = ts_node_child(tmp, i);
          if (start_found) {
            free(result->variable_array_length);
            result->variable_array_length = sp_struct_value(ctx, child);
            start_found                   = false;
          } else if (strcmp(ts_node_type(child), "[") == 0) {
            start_found = true;
          }
        } //for

        field_id = find_direct_chld_by_type(tmp, identifier);
        if (!ts_node_is_null(field_id)) {
          result->is_array = true;
          result->variable = sp_struct_value(ctx, field_id);
          /* TODO: '[' XXX ']' */
        }
        if (!result->variable_array_length) {
          result->variable_array_length = strdup("0");
        }
      } else {
        TSNode fun_decl;
        fun_decl = find_direct_chld_by_type(subject, "function_declarator");

        if (!ts_node_is_null(fun_decl)) {
          TSNode par_decl;
          par_decl =
            find_direct_chld_by_type(fun_decl, "parenthesized_declarator");

          fprintf(stderr, "%s: 1\n", __func__);
          if (!ts_node_is_null(par_decl)) {
            tmp = find_direct_chld_by_type(par_decl, "pointer_declarator");
            if (!ts_node_is_null(tmp)) {
              tmp = __rec_search(ctx, tmp, identifier, 1, &result->pointer);
              if (!ts_node_is_null(tmp)) {
                result->variable = sp_struct_value(ctx, tmp);
                fprintf(stderr, "||%s\n", result->variable);
                result->function_pointer = true;
              }
            }
          }
        } else {
          /* Note: this is for when we have `type var = "";` */
          tmp = find_direct_chld_by_type(subject, "init_declarator");
          if (!ts_node_is_null(tmp)) {
            return __field_name(ctx, tmp, identifier);
          } else {
          }
        }
      }
    }
  }

  return result;
}

static void
__format_numeric(struct arg_list *result,
                 const char *pprefix,
                 const char *format)
{
  char buffer[64] = {'\0'};
  if (result->pointer) {
    sp_str buf_tmp;

    sprintf(buffer, "%s%s", format, "%s");
    result->format = strdup(buffer);

    sp_str_init(&buf_tmp, 0);
    sp_str_appends(&buf_tmp, pprefix, result->variable, " ? *", pprefix,
                   result->variable, " : 0, ", NULL);
    sp_str_appends(&buf_tmp, pprefix, result->variable, " ? \"\" : \"(NULL)\"",
                   NULL);
    result->complex_raw    = strdup(sp_str_c_str(&buf_tmp));
    result->complex_printf = true;
    sp_str_free(&buf_tmp);
  } else {
    result->format = strdup(format);
  }
}

static void
__format(struct sp_ts_Context *ctx,
         struct arg_list *result,
         const char *pprefix)
{
  (void)ctx;
  /* https://developer.gnome.org/glib/stable/glib-Basic-Types.html */
  /* https://en.cppreference.com/w/cpp/types/integer */
  //TODO strdup
  if (result->dead) {
    return;
  }

  /* fprintf(stderr, "- %s\n", type); */
  if (result) {
    if (result->rec) {
      struct arg_list *it = result->rec;
      while (it) {
        sp_str buf_tmp;
        sp_str_init(&buf_tmp, 0);

        fprintf(stderr, "__%s:%s\n", it->variable, it->type);
        sp_str_appends(&buf_tmp, result->variable, ".", it->variable, NULL);

        free(it->variable);
        it->variable = strdup(sp_str_c_str(&buf_tmp));
        sp_str_free(&buf_tmp);

        if (!it->next) {
          break;
        }
        it = it->next;
      } //while
      it->next     = result->next;
      result->next = result->rec;
      result->dead = true;
      return;
    }
  }
  if (result->function_pointer) {
    if (ctx->domain == LINUX_KERNEL_DOMAIN && result->pointer == 1) {
      result->format = "%pF";
    } else {
      result->format = "%p";
    }
  } else if (result->type) {
    if (result->pointer > 1) {
      result->format = "%p";
    } else if (strcmp(result->type, "gboolean") == 0 || //
               strcmp(result->type, "bool") == 0 || //
               strcmp(result->type, "boolean") == 0) {
      sp_str buf_tmp;
      sp_str_init(&buf_tmp, 0);
      result->format = "%s";

      if (result->pointer) {
        sp_str_appends(&buf_tmp, "!", pprefix, result->variable,
                       " ? \"NULL\" : *", pprefix, result->variable, NULL);
      } else {
        sp_str_appends(&buf_tmp, pprefix, result->variable, NULL);
      }
      sp_str_appends(&buf_tmp, " ? \"TRUE\" : \"FALSE\"", NULL);
      free(result->complex_raw);
      result->complex_raw    = strdup(sp_str_c_str(&buf_tmp));
      result->complex_printf = true;

      sp_str_free(&buf_tmp);
    } else if (strcmp(result->type, "void") == 0) {
      if (result->pointer) {
        result->format = "%p";
      }
    } else if (strcmp(result->type, "gpointer") == 0) {
      sp_str buf_tmp;
      sp_str_init(&buf_tmp, 0);
      result->format = "%p";
      sp_str_appends(&buf_tmp, "(void*)", pprefix, result->variable, NULL);
      free(result->complex_raw);
      result->complex_raw    = strdup(sp_str_c_str(&buf_tmp));
      result->complex_printf = true;
      sp_str_free(&buf_tmp);
    } else if (strcmp(result->type, "string") == 0) {
      sp_str buf_tmp;
      sp_str_init(&buf_tmp, 0);

      /* Example: string */
      result->format = "%s";
      sp_str_appends(&buf_tmp, pprefix, result->variable, NULL);
      if (result->pointer) {
        sp_str_append(&buf_tmp, "->c_str()");
      } else {
        sp_str_append(&buf_tmp, ".c_str()");
      }
      free(result->complex_raw);
      result->complex_raw    = strdup(sp_str_c_str(&buf_tmp));
      result->complex_printf = true;

      sp_str_free(&buf_tmp);
    } else if (strcmp(result->type, "char") == 0 || //
               strcmp(result->type, "gchar") == 0 || //
               strcmp(result->type, "gint8") == 0 || //
               strcmp(result->type, "int8") == 0 || //
               strcmp(result->type, "int8_t") == 0) {
      if (result->is_array) {
        sp_str buf_tmp;
        result->format = "%.*s";

        sp_str_init(&buf_tmp, 0);
        /* char $field_identifier[$array_len] */
        sp_str_appends(&buf_tmp, "(int)", result->variable_array_length, ", ",
                       pprefix, result->variable, NULL);
        free(result->complex_raw);
        result->complex_raw    = strdup(sp_str_c_str(&buf_tmp));
        result->complex_printf = true;

        sp_str_free(&buf_tmp);
      } else if (result->pointer) {
        result->format = "%s";
      } else {
        result->format = "%c";
      }
    } else if (strcmp(result->type, "spinlock_t") == 0 ||
               strcmp(result->type, "pthread_spinlock_t") == 0 ||
               strcmp(result->type, "pthread_mutex_t") == 0 ||
               strcmp(result->type, "mutex_t") == 0 ||
               strcmp(result->type, "mutex") == 0 ||
               strcmp(result->type, "struct mutex") == 0) {
      /* fprintf(stderr, "type[%s]\n", type); */

    } else if (strcmp(result->type, "GError") == 0) {
      sp_str buf_tmp;
      sp_str_init(&buf_tmp, 0);

      result->format = "%s";
      if (result->pointer) {
        sp_str_appends(&buf_tmp, pprefix, result->variable, " ? ", pprefix,
                       result->variable, "->message : \"NULL\"", NULL);
      } else {
        sp_str_appends(&buf_tmp, pprefix, result->variable, ".message", NULL);
      }
      free(result->complex_raw);
      result->complex_raw    = strdup(sp_str_c_str(&buf_tmp));
      result->complex_printf = true;
      sp_str_free(&buf_tmp);
    } else if (strcmp(result->type, "GIOChannel") == 0) {
      sp_str buf_tmp;
      sp_str_init(&buf_tmp, 0);

      result->format = "%d";
      if (result->pointer) {
        sp_str_appends(&buf_tmp, pprefix, result->variable, " ? ",
                       "g_io_channel_unix_get_fd(", pprefix, result->variable,
                       ") : ", "-1337", NULL);
      } else {
        sp_str_appends(&buf_tmp, "g_io_channel_unix_get_fd(&", pprefix,
                       result->variable, ")", NULL);
      }
      free(result->complex_raw);
      result->complex_raw    = strdup(sp_str_c_str(&buf_tmp));
      result->complex_printf = true;
      sp_str_free(&buf_tmp);
    } else if (strcmp(result->type, "GKeyFile") == 0 ||
               strcmp(result->type, "GVariantBuilder") == 0) {
      sp_str buf_tmp;
      sp_str_init(&buf_tmp, 0);

      result->format = "%s";
      if (result->pointer) {
        sp_str_appends(&buf_tmp, pprefix, result->variable, " ? ", "\"SOME\"",
                       " : NULL", NULL);
      } else {
        sp_str_append(&buf_tmp, "\"SOME\"");
      }
      free(result->complex_raw);
      result->complex_raw    = strdup(sp_str_c_str(&buf_tmp));
      result->complex_printf = true;
      sp_str_free(&buf_tmp);
    } else if (strcmp(result->type, "time_t") == 0) {
      sp_str buf_tmp;
      result->format = "%s(%jd)";

      sp_str_init(&buf_tmp, 0);
      sp_str_appends(&buf_tmp, //
                     "asctime(", //
                     "gmtime(&", pprefix, result->variable, ")",
                     "), (intmax_t)", pprefix, result->variable, NULL);
      free(result->complex_raw);
      result->complex_raw    = strdup(sp_str_c_str(&buf_tmp));
      result->complex_printf = true;

      sp_str_free(&buf_tmp);
    } else if (strcmp(result->type, "IMFIX") == 0) {
      sp_str buf_tmp;
      sp_str_init(&buf_tmp, 0);
      result->format = "%f";
      if (result->pointer) {
        sp_str_appends(&buf_tmp, pprefix, result->variable, " ? ", "IMFIX2F(*",
                       pprefix, result->variable, ") : 0", NULL);
      } else {
        sp_str_appends(&buf_tmp, //
                       "IMFIX2F(", pprefix, result->variable, ")", NULL);
      }
      free(result->complex_raw);
      result->complex_raw    = strdup(sp_str_c_str(&buf_tmp));
      result->complex_printf = true;
      sp_str_free(&buf_tmp);

    } else if (strcmp(result->type, "sp_str") == 0) {
      sp_str buf_tmp;
      sp_str_init(&buf_tmp, 0);
      result->format = "%s";
      if (result->pointer) {
        sp_str_appends(&buf_tmp, pprefix, result->variable, " ? ",
                       "sp_str_c_str(", pprefix, result->variable, ")",
                       " : \"NULL\"", NULL);
      } else {
        sp_str_appends(&buf_tmp, "sp_str_c_str(&", pprefix, result->variable,
                       ")", NULL);
      }
      free(result->complex_raw);
      result->complex_raw    = strdup(sp_str_c_str(&buf_tmp));
      result->complex_printf = true;
      sp_str_free(&buf_tmp);

    } else if (strcmp(result->type, "GVariant") == 0) {
      sp_str buf_tmp;
      sp_str_init(&buf_tmp, 0);
      result->format = "%s";
      if (result->pointer) {
        sp_str_appends(&buf_tmp, pprefix, result->variable, " ? ",
                       "g_variant_print(", pprefix, result->variable,
                       ", FALSE)", " : \"NULL\"", NULL);
      } else {
        sp_str_appends(&buf_tmp, "g_variant_print(&", pprefix, result->variable,
                       ", FALSE)", NULL);
      }
      free(result->complex_raw);
      result->complex_raw    = strdup(sp_str_c_str(&buf_tmp));
      result->complex_printf = true;
      sp_str_free(&buf_tmp);
    } else if (strcmp(result->type, "GVariantIter") == 0) {
      sp_str buf_tmp;
      sp_str_init(&buf_tmp, 0);
      if (result->pointer) {
        result->format = "children[%zu]%s";
        sp_str_appends(&buf_tmp, pprefix, result->variable, " ? ",
                       "g_variant_iter_n_children(", pprefix, result->variable,
                       ")", " : 0", NULL);
        sp_str_appends(&buf_tmp, ", ", pprefix, result->variable, " ? ", "\"\"",
                       " : \"(NULL)\"", NULL);
      } else {
        result->format = "children[%zu]";
        sp_str_appends(&buf_tmp, "g_variant_iter_n_children(&", pprefix,
                       result->variable, ")", NULL);
      }
      free(result->complex_raw);
      result->complex_raw    = strdup(sp_str_c_str(&buf_tmp));
      result->complex_printf = true;
      sp_str_free(&buf_tmp);
    } else if (strcmp(result->type, "GArray") == 0) {
      sp_str buf_tmp;
      sp_str_init(&buf_tmp, 0);
      if (result->pointer) {
        result->format = "len[%u]%s";
        sp_str_appends(&buf_tmp, pprefix, result->variable, " ? ", pprefix,
                       result->variable, "->len", " : 0", NULL);
        sp_str_appends(&buf_tmp, ", ", pprefix, result->variable, " ? ", "\"\"",
                       " : \"(NULL)\"", NULL);
      } else {
        result->format = "len[%u]";
        sp_str_appends(&buf_tmp, pprefix, result->variable, ".len", NULL);
      }
      free(result->complex_raw);
      result->complex_raw    = strdup(sp_str_c_str(&buf_tmp));
      result->complex_printf = true;
      sp_str_free(&buf_tmp);

    } else if (strcmp(result->type, "uchar") == 0 || //
               strcmp(result->type, "unsigned char") == 0 || //
               strcmp(result->type, "guchar") == 0 || //
               strcmp(result->type, "guint8") == 0 || //
               strcmp(result->type, "uint8") == 0 || //
               strcmp(result->type, "u8") == 0 || //
               strcmp(result->type, "uint8_t") == 0) {
      if (result->pointer) {
        /* TODO if pointer hex? */
      } else if (result->is_array) {
        /* TODO if pointer hex? */
      } else {
        result->format = "%d";
      }
    } else if (strcmp(result->type, "short") == 0 || //
               strcmp(result->type, "gshort") == 0 || //
               strcmp(result->type, "gint16") == 0 || //
               strcmp(result->type, "int16") == 0 || //
               strcmp(result->type, "i16") == 0 || //
               strcmp(result->type, "int16_t") == 0) {
      __format_numeric(result, pprefix, "%d");
    } else if (strcmp(result->type, "unsigned short") == 0 || //
               strcmp(result->type, "gushort") == 0 || //
               strcmp(result->type, "guint16") == 0 || //
               strcmp(result->type, "uint16") == 0 || //
               strcmp(result->type, "u16") == 0 || //
               strcmp(result->type, "uint16_t") == 0) {
      __format_numeric(result, pprefix, "%u");
    } else if (strcmp(result->type, "int") == 0 || //
               strcmp(result->type, "signed int") == 0 || //
               strcmp(result->type, "gint") == 0 || //
               strcmp(result->type, "gint32") == 0 || //
               strcmp(result->type, "i32") == 0 || //
               strcmp(result->type, "int32") == 0 || //
               strcmp(result->type, "int32_t") == 0) {
      __format_numeric(result, pprefix, "%d");
    } else if (strcmp(result->type, "unsigned") == 0 || //
               strcmp(result->type, "unsigned int") == 0 || //
               strcmp(result->type, "guint") == 0 || //
               strcmp(result->type, "guint32") == 0 || //
               strcmp(result->type, "uint32") == 0 || //
               strcmp(result->type, "u32") == 0 || //
               strcmp(result->type, "uint32_t") == 0) {
      __format_numeric(result, pprefix, "%u");
    } else if (strcmp(result->type, "long") == 0 || //
               strcmp(result->type, "long int") == 0) {
      __format_numeric(result, pprefix, "%ld");
    } else if (strcmp(result->type, "unsigned long int") == 0 || //
               strcmp(result->type, "long unsigned int") == 0 || //
               strcmp(result->type, "unsigned long") == 0) {
      __format_numeric(result, pprefix, "%lu");
    } else if (strcmp(result->type, "long long") == 0 || //
               strcmp(result->type, "long long int") == 0) {
      __format_numeric(result, pprefix, "%lld");
    } else if (strcmp(result->type, "unsigned long long") == 0 || //
               strcmp(result->type, "unsigned long long int") == 0) {
      __format_numeric(result, pprefix, "%llu");
    } else if (strcmp(result->type, "off_t") == 0) {
      __format_numeric(result, pprefix, "%jd");
    } else if (strcmp(result->type, "goffset") == 0) {
      result->format = "%\"G_GOFFSET_FORMAT\"";
    } else if (strcmp(result->type, "size_t") == 0) {
      __format_numeric(result, pprefix, "%zu");
    } else if (strcmp(result->type, "gsize") == 0) {
      result->format = "%\"G_GSIZE_FORMAT\"";
    } else if (strcmp(result->type, "ssize_t") == 0) {
      __format_numeric(result, pprefix, "%zd");
    } else if (strcmp(result->type, "gssize") == 0) {
      result->format = "%\"G_GSSIZE_FORMAT\"";
    } else if (strcmp(result->type, "int64_t") == 0) {
      result->format = "%\"PRId64\"";
    } else if (strcmp(result->type, "uint64_t") == 0) {
      result->format = "%\"PRIu64\"";
    } else if (strcmp(result->type, "guint64") == 0) {
      result->format = "%\"G_GUINT64_FORMAT\"";
    } else if (strcmp(result->type, "uintptr_t") == 0) {
      result->format = "%\"PRIuPTR\"";
    } else if (strcmp(result->type, "guintptr") == 0) {
      result->format = "%\"G_GUINTPTR_FORMAT\"";
    } else if (strcmp(result->type, "iintptr_t") == 0) {
      result->format = "%\"PRIiPTR\"";
    } else if (strcmp(result->type, "gintptr") == 0) {
      result->format = "%\"G_GINTPTR_FORMAT\"";
    } else if (strcmp(result->type, "float") == 0 || //
               strcmp(result->type, "gfloat") == 0 || //
               strcmp(result->type, "double") == 0 || //
               strcmp(result->type, "gdouble") == 0) {
      __format_numeric(result, pprefix, "%f");
    } else if (strcmp(result->type, "long double") == 0) {
      __format_numeric(result, pprefix, "%Lf");
    } else {
      if (strchr(result->type, ' ') == NULL) {
        const char *prefix = "&";
        sp_str buf_tmp;
        sp_str_init(&buf_tmp, 0);

        /* Example: type_t */
        result->format = "%s";
        if (result->pointer) {
          prefix = "";
        }
        sp_str_appends(&buf_tmp, "sp_debug_", result->type, "(", NULL);
        sp_str_appends(&buf_tmp, prefix, pprefix, result->variable, NULL);
        sp_str_appends(&buf_tmp, ")", NULL);
        free(result->complex_raw);
        result->complex_raw    = strdup(sp_str_c_str(&buf_tmp));
        result->complex_printf = true;

        sp_str_free(&buf_tmp);
      } else {
        assert(false);
      }
    }
  }
}

static struct arg_list *
__parameter_to_arg(struct sp_ts_Context *ctx, TSNode subject)
{
  struct arg_list *result = NULL;

  if ((result = __field_name(ctx, subject, "identifier"))) {
    /* fprintf(stderr, "|%s\n", result->variable); */
    result->type = __field_type(ctx, subject, result, "");
    /* fprintf(stderr, "|%s: %s\n", result->type, result->variable); */
    __format(ctx, result, "");
  }

  if (result && result->format && result->variable) {
    result->complete = true;
  }

  return result;
}

static int
sp_do_print_function(struct sp_ts_Context *ctx, struct arg_list *const fields)
{
  const size_t MAX_LINE = 75;
  sp_str buf;
  sp_str line_buf;
  size_t line_length;
  size_t complete = 0;
  struct arg_list *field_it;
  sp_str_init(&buf, 0);
  sp_str_init(&line_buf, 0);

  if (ctx->domain == DEFAULT_DOMAIN) {
    sp_str_append(&buf, "  printf(");
  } else if (ctx->domain == SYSLOG_DOMAIN) {
    sp_str_append(&buf, "  syslog(LOG_ERR,");
  } else if (ctx->domain == LINUX_KERNEL_DOMAIN) {
    sp_str_append(&buf, "  printk(KERN_ERR ");
  }
  //TODO limit the length of the line when printing the format part AND maybe for alignment of the variable part by //
  sp_str_append(&buf, "\"%s:");
  line_length = sp_str_length(&buf);
  field_it    = fields;
  while (field_it) {
    if (field_it->complete) {
      sp_str_appends(&line_buf, field_it->variable, "[", field_it->format, "]",
                     NULL);
      ++complete;
    } else {
      fprintf(stderr, "%s: Incomplete: %s\n", __func__,
              field_it->variable ? field_it->variable : "NULL");
    }
    field_it = field_it->next;
    if ((line_length + sp_str_length(&line_buf)) > MAX_LINE) {
      sp_str_append(&buf, "\" //\n\"");
      line_length = 0;
    }
    sp_str_append_str(&buf, &line_buf);
    line_length += sp_str_length(&line_buf);
    sp_str_clear(&line_buf);
  } //while

  sp_str_append(&buf, "\\n\", __func__");
  field_it = fields;
  while (field_it) {
    if (field_it->complete) {
      sp_str_append(&buf, ", ");
      if (field_it->complex_printf) {
        assert(field_it->complex_raw);
        sp_str_append(&buf, field_it->complex_raw);
      } else {
        sp_str_append(&buf, field_it->variable);
      }
    }
    field_it = field_it->next;
  } //while
  sp_str_append(&buf, ");");

  fprintf(stdout, "%s", sp_str_c_str(&buf));
  sp_str_free(&line_buf);
  sp_str_free(&buf);

  return EXIT_SUCCESS;
}

static int
sp_print_function_args(struct sp_ts_Context *ctx, TSNode subject)
{
  int res = EXIT_SUCCESS;
  TSNode tmp;
  struct arg_list field_dummy = {0};
  struct arg_list *field_it   = &field_dummy;

  /* printf("here!"); */
  tmp = find_direct_chld_by_type(subject, "function_declarator");
  if (!ts_node_is_null(tmp)) {
    tmp = find_direct_chld_by_type(tmp, "parameter_list");
    if (!ts_node_is_null(tmp)) {
      uint32_t i;
      struct arg_list *arg = NULL;

      for (i = 0; i < ts_node_child_count(tmp); ++i) {
        TSNode param_decl = ts_node_child(tmp, i);
        if (strcmp(ts_node_type(param_decl), "parameter_declaration") == 0) {
#if 0
  uint32_t a;
  for (a = 0; a < ts_node_child_count(param_decl); ++a) {
    TSNode child = ts_node_child(param_decl, a);
    printf("- %s\n", ts_node_string(child));
  }
#endif
          if ((arg = __parameter_to_arg(ctx, param_decl))) {
            field_it = field_it->next = arg;
          }
        }
      } //for
    } else {
      printf("null\n");
    }

  } else {
    /* printf("null\n"); */
  }

  sp_do_print_function(ctx, field_dummy.next);
  return res;
}
static TSNode
sp_find_sibling_of_type(TSNode subject, const char *type)
{
  TSNode empty = {0};
  TSNode it;
  if (strcmp(ts_node_type(subject), type) == 0) {
    return subject;
  }

  it = subject;
  {
  loop1:
    it = ts_node_prev_sibling(it);

    if (!ts_node_is_null(it)) {
      if (strcmp(ts_node_type(it), type) == 0) {
        return subject;
      }
      goto loop1;
    }
  }

  it = subject;
  {
  loop2:
    it = ts_node_next_sibling(it);

    if (!ts_node_is_null(it)) {
      if (strcmp(ts_node_type(it), type) == 0) {
        return subject;
      }
      goto loop2;
    }
  }

  return empty;
}

static int
sp_print_locals(struct sp_ts_Context *ctx, TSNode subject)
{
  int res                     = EXIT_SUCCESS;
  TSNode it                   = subject;
  struct arg_list field_dummy = {0};
  struct arg_list *field_it   = &field_dummy;
  while (!ts_node_is_null(it)) {
    TSNode sibling = it;
    /* fprintf(stderr, "%s\n", ts_node_type(it)); */
    /* fprintf(stderr, "- %s\n", ts_node_type(it)); */
    if (!ts_node_is_null(sp_find_sibling_of_type(it, "function_definition"))) {
      break;
    }
    do {
      if (strcmp(ts_node_type(sibling), "declaration") == 0) {
        struct arg_list *arg = NULL;
#if 0
        uint32_t s           = ts_node_start_byte(sibling);
        uint32_t e           = ts_node_end_byte(sibling);
        uint32_t len         = e - s;
        fprintf(stderr, "%.*s: %s\n", (int)len, &ctx->file.content[s],
                ts_node_type(sibling));
#endif
        if ((arg = __parameter_to_arg(ctx, sibling))) {
          field_it = field_it->next = arg;
        }
        //TODO maybe add is_initated:bool to struct arg_list (which is true for function parametrs, unrelevant for struct members)
        //  TODO then add support to detect for variable init after declaration
      }
      sibling = ts_node_prev_sibling(sibling);
    } while (!ts_node_is_null(sibling));

    it = ts_node_parent(it);
  } //while
  //TODO reverse list before printing
  /* fprintf(stderr, "============================\n"); */
  /* debug_subtypes_rec(ctx, it, 0); */
  sp_do_print_function(ctx, field_dummy.next);

  /*     TODO int i, j; */
  /* TODO cursor on empty line */

  return res;
}

static struct arg_list *
__field_to_arg(struct sp_ts_Context *ctx, TSNode subject, const char *pprefix)
{
  struct arg_list *result = NULL;

  /* fprintf(stderr, "%s\n", __func__); */

  /* TODO result->format, result->variable strdup() */

  if ((result = __field_name(ctx, subject, "field_identifier"))) {
    struct arg_list *it;
    /* fprintf(stderr,"%s\n", result->variable); */
    result->type = __field_type(ctx, subject, result, pprefix);
    /* fprintf(stderr, "type[%s]\n", type); */

    it = result;
    while (1) {
      __format(ctx, it, pprefix);
      it->complete = false;
      if (!it->dead && it->format && it->variable) {
        it->complete = true;
      }
      if (!it->next) {
        break;
      }
      it = it->next;
    }
  }

  return result;
}

static int
sp_do_print_struct(struct sp_ts_Context *ctx,
                   bool type_name_t,
                   const char *type_name,
                   struct arg_list *const fields,
                   const char *pprefix)
{
  struct arg_list *field_it;
  size_t complete = 0;
  sp_str buf;
  sp_str_init(&buf, 0);

  (void)ctx;

  sp_str_appends(&buf, "static inline const char* sp_debug_", type_name, "(",
                 NULL);
  sp_str_appends(&buf, "const ", type_name_t ? "" : "struct ", type_name, " *",
                 pprefix, NULL);
  sp_str_append(&buf, ") {\n");
  sp_str_append(&buf, "  static char buf[1024] = {'\\0'};\n");
  sp_str_appends(&buf, "  if (!", pprefix, ") return \"", type_name,
                 "(NULL)\";\n", NULL);
  field_it = fields;
  sp_str_appends(&buf, "  snprintf(buf, sizeof(buf), \"", type_name, "(%p){",
                 NULL);
  while (field_it) {
    if (field_it->complete) {
      sp_str_appends(&buf, field_it->variable, "[", field_it->format, "]",
                     NULL);
      ++complete;
    } else {
      fprintf(stderr, "%s: Incomplete: %s\n", __func__,
              field_it->variable ? field_it->variable : "NULL");
    }
    field_it = field_it->next;
  } //while
  sp_str_appends(&buf, "}\", ", pprefix, NULL);

  field_it = fields;
  while (field_it) {
    if (field_it->complete) {
      sp_str_append(&buf, ", ");
      if (field_it->complex_printf) {
        assert(field_it->complex_raw);
        sp_str_append(&buf, field_it->complex_raw);
      } else {
        sp_str_appends(&buf, pprefix, "->", field_it->variable, NULL);
      }
    }
    field_it = field_it->next;
  } //while
  sp_str_append(&buf, ");\n");
  sp_str_append(&buf, "  return buf;\n");
  sp_str_append(&buf, "}\n");

  fprintf(stdout, "%s", sp_str_c_str(&buf));

  sp_str_free(&buf);
  return EXIT_SUCCESS;
}

static int
sp_print_struct(struct sp_ts_Context *ctx, TSNode subject)
{
  char *type_name             = NULL;
  bool type_name_t            = false;
  struct arg_list field_dummy = {0};
  struct arg_list *field_it   = &field_dummy;
  uint32_t i;
  TSNode tmp;
  const char *pprefix  = "in->";
  const char *pprefix2 = "in";

  tmp = find_direct_chld_by_type(subject, "type_identifier");
  if (!ts_node_is_null(tmp)) {
    /* struct type_name { ... }; */
    type_name = sp_struct_value(ctx, tmp);
  } else {
    TSNode parent = ts_node_parent(subject);
    if (!ts_node_is_null(parent)) {
      if (strcmp(ts_node_type(parent), "type_definition") == 0) {
        tmp = find_direct_chld_by_type(parent, "type_identifier");
        if (!ts_node_is_null(tmp)) {
          /* typedef struct * { ... } type_name; */
          type_name_t = true;
          type_name   = sp_struct_value(ctx, tmp);
        }
      }
    }
  }

  tmp = find_direct_chld_by_type(subject, "field_declaration_list");
  if (!ts_node_is_null(tmp)) {
    for (i = 0; i < ts_node_child_count(tmp); ++i) {
      TSNode field = ts_node_child(tmp, i);
      /* fprintf(stderr, "i.%u\n", i); */
      if (strcmp(ts_node_type(field), "field_declaration") == 0) {
        struct arg_list *arg = NULL;

        if ((arg = __field_to_arg(ctx, field, pprefix))) {
          field_it->next = arg;
          while (field_it->next) {
            field_it = field_it->next;
          }
        }
      }
    } //for
  }

  sp_do_print_struct(ctx, type_name_t, type_name, field_dummy.next, pprefix2);
  free(type_name);
  return EXIT_SUCCESS;
}

static int
sp_print_class(struct sp_ts_Context *ctx, TSNode subject)
{
  return EXIT_SUCCESS;
}

static uint32_t
sp_find_last_line(TSNode subject)
{
  TSPoint p = ts_node_end_point(subject);
  return p.row + 1;
}

static uint32_t
sp_find_open_bracket(TSNode subject)
{
  TSPoint p;
  TSNode body;

  body = find_direct_chld_by_type(subject, "compound_statement");
  if (!ts_node_is_null(body)) {
    p = ts_node_start_point(body);
  } else {
    p = ts_node_end_point(subject);
  }
  return p.row + 1;
  /*             for (i = 0; i < ts_node_child_count(tmp); ++i) { */
  /*             } */
}

static int
main_print(const char *in_file)
{
  int res                  = EXIT_FAILURE;
  struct sp_ts_Context ctx = {0};
  if (mmap_file(in_file, &ctx.file) == 0) {
    TSNode root;
    TSParser *parser       = ts_parser_new();
    const TSLanguage *lang = tree_sitter_c();
    ts_parser_set_language(parser, lang);

    ctx.tree = ts_parser_parse_string(parser, NULL, ctx.file.content,
                                      (uint32_t)ctx.file.length);
    if (!ctx.tree) {
      fprintf(stderr, "failed to parse\n");
      goto Lerr;
    }

    root = ts_tree_root_node(ctx.tree);
    if (!ts_node_is_null(root)) {
      printf("%s\n", ts_node_string(root));
    } else {
      goto Lerr;
    }
  }
  res = EXIT_SUCCESS;
Lerr:
  return res;
}

static TSNode
closest_before_pos(TSPoint pos, TSNode closest, TSNode candidate)
{
  TSPoint cand_point = ts_node_start_point(candidate);
  if (ts_node_is_null(closest)) {
    closest = candidate;
  } else {
    TSPoint closest_point = ts_node_start_point(closest);
    if (cand_point.row <= pos.row && closest_point.row > pos.row) {
      closest = candidate;
    } else if (abs((int)pos.row - (int)cand_point.row) <
               abs((int)pos.row - (int)closest_point.row)) {
      closest = candidate;
    } else if (cand_point.row == pos.row && cand_point.row == pos.row) {
      if (closest_point.column > cand_point.column &&
          closest_point.column > pos.column) {
        closest = candidate;
      } else if (closest_point.column < pos.column &&
                 cand_point.column < pos.column) {
        if (cand_point.column > closest_point.column) {
          closest = candidate;
        }
      }
      //...
    }
  }
  /* This is a mess */
  return closest;
}

int
main(int argc, const char *argv[])
{
  int res                  = EXIT_FAILURE;
  TSPoint pos              = {.row = 0, .column = 0};
  struct sp_ts_Context ctx = {0};
  const char *in_type      = NULL;
  const char *in_file      = NULL;
  const char *in_line      = NULL;
  const char *in_column    = NULL;

  if (argc != 5) {
    if (argc > 1) {
      in_type = argv[1];
      if (argc == 3 && strcmp(in_type, "print") == 0) {
        in_file = argv[2];
        return main_print(in_file);
      }
    }
    fprintf(stderr, "%s crunch|line|print file [line] [column]\n", argv[0]);
    return EXIT_FAILURE;
  }
  in_type   = argv[1];
  in_file   = argv[2];
  in_line   = argv[3];
  in_column = argv[4];

  if (!sp_parse_uint32_t(in_line, &pos.row)) {
    fprintf(stderr, "failed to parse line '%s'\n", in_line);
    return EXIT_FAILURE;
  }
  if (!sp_parse_uint32_t(in_column, &pos.column)) {
    fprintf(stderr, "failed to parse column '%s'\n", in_column);
    return EXIT_FAILURE;
  }

  if (mmap_file(in_file, &ctx.file) == 0) {
    TSParser *parser          = ts_parser_new();
    const TSLanguage *clang   = tree_sitter_c();
    const TSLanguage *cpplang = tree_sitter_cpp();
    if (is_cpp_file(in_file)) {
      ts_parser_set_language(parser, cpplang);
    } else if (is_c_file(in_file)) {
      ts_parser_set_language(parser, clang);
    } else {
      ts_parser_set_language(parser, clang);
    }
    ctx.domain = get_domain(in_file);

    {
      TSNode root;
      ctx.tree = ts_parser_parse_string(parser, NULL, ctx.file.content,
                                        (uint32_t)ctx.file.length);
      if (!ctx.tree) {
        fprintf(stderr, "failed to parse\n");
        goto Lerr;
      }

      /* ts_tree_print_dot_graph(tree, stdout); */
      root = ts_tree_root_node(ctx.tree);
      if (!ts_node_is_null(root)) {
        TSNode highligted;
#if 0
        {
          uint32_t i;
          char *p = ts_node_string(root);
          fprintf(stdout, "%s\n", p);
          free(p);
          TSNode tmp = find_direct_chld_by_type(root, "declaration");
          if (!ts_node_is_null(tmp)) {
            for (i = 0; i < ts_node_child_count(tmp); ++i) {
              uint32_t s   = ts_node_start_byte(ts_node_child(tmp, i));
              uint32_t e   = ts_node_end_byte(ts_node_child(tmp, i));
              uint32_t len = e - s;
              fprintf(stderr, ".%u\n", i);
              /* fprintf(stderr, "s: %u\n", s); */
              /* fprintf(stderr, "e: %u\n", e); */
              /* fprintf(stderr, "len: %u\n", len); */
              fprintf(stderr, "%.*s: %s\n", (int)len, &ctx.file.content[s],
                      ts_node_type(ts_node_child(tmp, i)));
            }
          }
        }
#endif

        highligted = ts_node_descendant_for_point_range(root, pos, pos);
        if (!ts_node_is_null(highligted)) {
          if (strcmp(in_type, "locals") == 0) {
            TSPoint hpoint = ts_node_start_point(highligted);
            if (hpoint.row != pos.row) {
              TSNode closest           = {0};
              struct list_TSNode dummy = {0};
              struct list_TSNode *it;
              __leafs(&ctx, highligted, &dummy);
              it = dummy.next;
              while (it) {
                struct list_TSNode *tmp = it;
                closest = closest_before_pos(pos, closest, it->node);
                it      = it->next;
                free(tmp);
              }
              if (!ts_node_is_null(closest)) {
                highligted = closest;
              }
            }

            if (strcmp(ts_node_type(highligted), "}") == 0) {
              highligted = ts_node_parent(highligted);
            }
            /* debug_subtypes_rec(&ctx, highligted, 0); */
            res = sp_print_locals(&ctx, highligted);
          } else {
            const char *struct_spec = "struct_specifier";
            const char *class_spec  = "class_specifier";
            const char *enum_spec   = "enum_specifier";
            const char *fun_spec    = "function_definition";
            TSNode found = sp_find_parent(highligted, struct_spec, enum_spec,
                                          fun_spec, class_spec);
            if (!ts_node_is_null(found)) {
              if (strcmp(ts_node_type(found), struct_spec) == 0) {
                if (strcmp(in_type, "crunch") == 0) {
                  res = sp_print_struct(&ctx, found);
                } else {
                  uint32_t line;
                  line = sp_find_last_line(found);
                  fprintf(stdout, "%u", line);
                  res = EXIT_SUCCESS;
                }
              }
#if 1
              else if (strcmp(ts_node_type(found), class_spec) == 0) {
                if (strcmp(in_type, "crunch") == 0) {
                  res = sp_print_class(&ctx, found);
                } else {
                  uint32_t line;
                  line = sp_find_last_line(found);
                  fprintf(stdout, "%u", line);
                  res = EXIT_SUCCESS;
                }
              }
#endif
              else if (strcmp(ts_node_type(found), enum_spec) == 0) {
                if (strcmp(in_type, "crunch") == 0) {
                  res = sp_print_enum(&ctx, found);
                } else {
                  uint32_t line;
                  line = sp_find_last_line(found);
                  fprintf(stdout, "%u", line);
                  res = EXIT_SUCCESS;
                }
              } else if (strcmp(ts_node_type(found), fun_spec) == 0) {
                if (strcmp(in_type, "crunch") == 0) {
                  /* printf("%s\n", ts_node_string(found)); */
                  res = sp_print_function_args(&ctx, found);
                } else {
                  uint32_t line;
                  line = sp_find_open_bracket(found);
                  fprintf(stdout, "%u", line);
                  res = EXIT_SUCCESS;
                }
              }
            }
          }
        } else {
          fprintf(stderr, "out of range %u,%u\n", pos.row, pos.column);
        }
      } else {
        fprintf(stderr, "Tree is empty \n");
      }
      ts_tree_delete(ctx.tree);
    }

  Lerr:
    ts_parser_delete(parser);
    /* munmap(file, flength); */
  }

  return res;
}

/* TODO support complex (json) output for line and crunch */
//TODO print all local variables visible at cursor position (leader+l)
//TODO print indiciation of in which if case we are located in
//TODO print indication before return

//TODO when we make assumption example (unsigned char*xxx, size_t l_xxx) make a comment in the debug function
// example: NOTE: assumes xxx and l_xxx is related

// TODO generate 2 print function typedef struct name {} name_t;
//
// TODO enum bitset FIRST=1 SECOND=2 THRID=4, var=FIRST|SECOND
//      if enum has explicit values and if binary they do not owerlap = assume enum bitset
