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
sp_find_direct_child_by_type(TSNode subject, const char *needle)
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

  sp_str_init(&buf, 0);

  /* fprintf(stderr, "%s\n", __func__); */

  tmp = sp_find_direct_child_by_type(subject, "type_identifier");
  if (!ts_node_is_null(tmp)) {
    /* struct type_name { ... }; */
    type_name = sp_struct_value(ctx, tmp);
  } else {
    TSNode parent = ts_node_parent(subject);
    if (!ts_node_is_null(parent)) {
      if (strcmp(ts_node_type(parent), "type_definition") == 0) {
        tmp = sp_find_direct_child_by_type(parent, "type_identifier");
        if (!ts_node_is_null(tmp)) {
          /* typedef struct * { ... } type_name; */
          type_name_t = true;
          type_name   = sp_struct_value(ctx, tmp);
        }
      }
    }
  }

  tmp = sp_find_direct_child_by_type(subject, "enumerator_list");
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

struct arg_list;
struct arg_list {
  char *format;
  char *variable;
  bool complete;
  char *complex_raw;
  bool complex_printf;
  uint32_t pointer;

  bool is_array;
  char *variable_array_length;

  struct arg_list *next;
};

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
             const char *print_prefix)
{
  TSNode tmp;
  char *type = NULL;

  tmp = sp_find_direct_child_by_type(subject, "primitive_type");
  if (!ts_node_is_null(tmp)) {
    /* $primitive_type $field_identifier; */
    type = sp_struct_value(ctx, tmp);
  } else {
    tmp = sp_find_direct_child_by_type(subject, "sized_type_specifier");
    if (!ts_node_is_null(tmp)) {
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
      tmp = sp_find_direct_child_by_type(subject, "type_identifier");
      if (!ts_node_is_null(tmp)) {
        /* $type_identifier $field_identifier;
         * Example:
         *  type_t type0;
         *  gint int0;
         */
        type = sp_struct_value(ctx, tmp);
      } else {
        tmp = sp_find_direct_child_by_type(subject, "enum_specifier");
        if (!ts_node_is_null(tmp)) {
          TSNode type_id;

          type_id = sp_find_direct_child_by_type(tmp, "type_identifier");
          if (!ts_node_is_null(type_id)) {
            result->format = "%s";
            if (result->pointer > 1) {
            } else {
              char *enum_type    = sp_struct_value(ctx, type_id);
              const char *prefix = "&";
              sp_str buf_tmp;
              sp_str_init(&buf_tmp, 0);

              /* Example: enum type_t */
              if (result->pointer) {
                prefix = "";
              }
              sp_str_appends(&buf_tmp, "sp_debug_", enum_type, "(", prefix,
                             print_prefix, result->variable, ")", NULL);
              result->complex_raw    = strdup(sp_str_c_str(&buf_tmp));
              result->complex_printf = true;

              /* enum $type_identifier $field_identifier; */
              // sp_debug_$enum_type($var->($result->variable));
              sp_str_free(&buf_tmp);
              free(enum_type);
            }
          } else {
            TSNode enum_list;

            enum_list = sp_find_direct_child_by_type(tmp, "enumerator_list");
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
                sp_str_appends(&buf_tmp, print_prefix, result->variable,
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
          tmp = sp_find_direct_child_by_type(subject, "struct_specifier");
          if (!ts_node_is_null(tmp)) {
#if 0
            fprintf(stderr, "5\n");
            {
              uint32_t i;
              for (i = 0; i < ts_node_child_count(tmp); ++i) {
                TSNode child          = ts_node_child(tmp, i);
                const char *node_type = ts_node_type(child);
                fprintf(stderr, "# [%s]\n", node_type);
              }
            }
#endif

            tmp = sp_find_direct_child_by_type(tmp, "type_identifier");
            if (!ts_node_is_null(tmp)) {
              type = sp_struct_value(ctx, tmp);
            } else {
              tmp = sp_find_direct_child_by_type(subject, "field_identifier");
              if (!ts_node_is_null(tmp)) {
#if 0
                {
                  unsigned i;
                  for (i = 0; i < ts_node_child_count(tmp); ++i) {
                    TSNode child = ts_node_child(tmp, i);
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
#endif
                uint32_t s   = ts_node_start_byte(subject);
                uint32_t e   = ts_node_end_byte(subject);
                uint32_t len = e - s;
                fprintf(stderr, "%.*s: %s\n", (int)len, &ctx->file.content[s],
                        ts_node_type(subject));
              } else {
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

  tmp = sp_find_direct_child_by_type(subject, identifier);
  if (!ts_node_is_null(tmp)) {
    result->variable = sp_struct_value(ctx, tmp);
    /* fprintf(stderr, "1: %s\n", result->variable); */
  } else {
    tmp = sp_find_direct_child_by_type(subject, "pointer_declarator");
    if (!ts_node_is_null(tmp)) {
      tmp = __rec_search(ctx, tmp, identifier, 1, &result->pointer);
      if (!ts_node_is_null(tmp)) {
        result->variable = sp_struct_value(ctx, tmp);
      }
    } else {
      tmp = sp_find_direct_child_by_type(subject, "array_declarator");
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

        field_id = sp_find_direct_child_by_type(tmp, identifier);
        if (!ts_node_is_null(field_id)) {
          result->is_array = true;
          result->variable = sp_struct_value(ctx, field_id);
          /* TODO: '[' XXX ']' */
        }
        if (!result->variable_array_length) {
          result->variable_array_length = strdup("0");
        }
      }
    }
  }

  return result;
}

static void
__format_numeric(struct arg_list *result,
                 const char *print_prefix,
                 const char *format)
{
  char buffer[64] = {'\0'};
  if (result->pointer) {
    sp_str buf_tmp;

    sprintf(buffer, "%s%s", format, "%s");
    result->format = strdup(buffer);

    sp_str_init(&buf_tmp, 0);
    sp_str_appends(&buf_tmp, print_prefix, result->variable, " ? *",
                   print_prefix, result->variable, " : 1337, ", NULL);
    sp_str_appends(&buf_tmp, print_prefix, result->variable,
                   " ? \"\" : \"NULL\"", NULL);
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
         const char *type,
         const char *print_prefix)
{
  (void)ctx;
  /* https://developer.gnome.org/glib/stable/glib-Basic-Types.html */
  /* https://en.cppreference.com/w/cpp/types/integer */
  //TODO strdup

  /* fprintf(stderr, "- %s\n", type); */
  if (type) {
    if (result->pointer > 1) {
      result->format = "%p";
    } else if (strcmp(type, "gboolean") == 0 || //
               strcmp(type, "bool") == 0 || //
               strcmp(type, "boolean") == 0) {
      sp_str buf_tmp;
      sp_str_init(&buf_tmp, 0);
      result->format = "%s";

      if (result->pointer) {
        sp_str_appends(&buf_tmp, "!", print_prefix, result->variable,
                       " ? \"NULL\" : *", print_prefix, result->variable, NULL);
      } else {
        sp_str_appends(&buf_tmp, print_prefix, result->variable, NULL);
      }
      sp_str_appends(&buf_tmp, " ? \"TRUE\" : \"FALSE\"", NULL);
      result->complex_raw    = strdup(sp_str_c_str(&buf_tmp));
      result->complex_printf = true;

      sp_str_free(&buf_tmp);
    } else if (strcmp(type, "void") == 0) {
      if (result->pointer) {
        result->format = "%p";
      }
    } else if (strcmp(type, "gpointer") == 0) {
      sp_str buf_tmp;
      sp_str_init(&buf_tmp, 0);
      result->format = "%p";
      sp_str_appends(&buf_tmp, "(void*)", print_prefix, result->variable, NULL);
      result->complex_raw    = strdup(sp_str_c_str(&buf_tmp));
      result->complex_printf = true;
      sp_str_free(&buf_tmp);
    } else if (strcmp(type, "string") == 0) {
      sp_str buf_tmp;
      sp_str_init(&buf_tmp, 0);

      /* Example: string */
      result->format = "%s";
      sp_str_appends(&buf_tmp, print_prefix, result->variable, NULL);
      if (result->pointer) {
        sp_str_append(&buf_tmp, "->c_str()");
      } else {
        sp_str_append(&buf_tmp, ".c_str()");
      }
      result->complex_raw    = strdup(sp_str_c_str(&buf_tmp));
      result->complex_printf = true;

      sp_str_free(&buf_tmp);
    } else if (strcmp(type, "char") == 0 || //
               strcmp(type, "gchar") == 0 || //
               strcmp(type, "gint8") == 0 || //
               strcmp(type, "int8") == 0 || //
               strcmp(type, "int8_t") == 0) {
      if (result->is_array) {
        sp_str buf_tmp;
        result->format = "%.*s";

        sp_str_init(&buf_tmp, 0);
        /* char $field_identifier[$array_len] */
        sp_str_appends(&buf_tmp, "(int)", result->variable_array_length, ", ",
                       print_prefix, result->variable, NULL);
        result->complex_raw    = strdup(sp_str_c_str(&buf_tmp));
        result->complex_printf = true;

        sp_str_free(&buf_tmp);
      } else if (result->pointer) {
        result->format = "%s";
      } else {
        result->format = "%c";
      }
    } else if (strcmp(type, "spinlock_t") == 0 ||
               strcmp(type, "pthread_spinlock_t") == 0 ||
               strcmp(type, "pthread_mutex_t") == 0 ||
               strcmp(type, "mutex_t") == 0 || strcmp(type, "mutex") == 0 ||
               strcmp(type, "struct mutex") == 0) {
      /* fprintf(stderr, "type[%s]\n", type); */
    } else if (strcmp(type, "time_t") == 0) {
      sp_str buf_tmp;
      result->format = "%s(%jd)";

      sp_str_init(&buf_tmp, 0);
      sp_str_appends(&buf_tmp, "asctime(gmtime(&", print_prefix,
                     result->variable, ")), (intmax_t)", print_prefix,
                     result->variable, NULL);
      result->complex_raw    = strdup(sp_str_c_str(&buf_tmp));
      result->complex_printf = true;

      sp_str_free(&buf_tmp);
    } else if (strcmp(type, "IMFIX") == 0) {
      sp_str buf_tmp;
      sp_str_init(&buf_tmp, 0);
      result->format = "%f";
      if (result->pointer) {
        sp_str_appends(&buf_tmp, print_prefix, result->variable, " ? IMFIX2F(*",
                       print_prefix, result->variable, ") : 0", NULL);
      } else {
        sp_str_appends(&buf_tmp, "IMFIX2F(", print_prefix, result->variable,
                       ")", NULL);
      }
      result->complex_raw    = strdup(sp_str_c_str(&buf_tmp));
      result->complex_printf = true;
      sp_str_free(&buf_tmp);
    } else if (strcmp(type, "uchar") == 0 || //
               strcmp(type, "guchar") == 0 || //
               strcmp(type, "guint8") == 0 || //
               strcmp(type, "uint8") == 0 || //
               strcmp(type, "u8") == 0 || //
               strcmp(type, "uint8_t") == 0) {
      result->format = "%d";
      /* TODO if pointer hex? */
    } else if (strcmp(type, "short") == 0 || //
               strcmp(type, "gshort") == 0 || //
               strcmp(type, "gint16") == 0 || //
               strcmp(type, "int16") == 0 || //
               strcmp(type, "i16") == 0 || //
               strcmp(type, "int16_t") == 0) {
      __format_numeric(result, print_prefix, "%d");
    } else if (strcmp(type, "unsigned short") == 0 || //
               strcmp(type, "gushort") == 0 || //
               strcmp(type, "guint16") == 0 || //
               strcmp(type, "uint16") == 0 || //
               strcmp(type, "u16") == 0 || //
               strcmp(type, "uint16_t") == 0) {
      __format_numeric(result, print_prefix, "%u");
    } else if (strcmp(type, "int") == 0 || //
               strcmp(type, "signed int") == 0 || //
               strcmp(type, "gint") == 0 || //
               strcmp(type, "gint32") == 0 || //
               strcmp(type, "i32") == 0 || //
               strcmp(type, "int32") == 0 || //
               strcmp(type, "int32_t") == 0) {
      __format_numeric(result, print_prefix, "%d");
    } else if (strcmp(type, "unsigned") == 0 || //
               strcmp(type, "unsigned int") == 0 || //
               strcmp(type, "guint") == 0 || //
               strcmp(type, "guint32") == 0 || //
               strcmp(type, "uint32") == 0 || //
               strcmp(type, "u32") == 0 || //
               strcmp(type, "uint32_t") == 0) {
      __format_numeric(result, print_prefix, "%u");
    } else if (strcmp(type, "long") == 0 || //
               strcmp(type, "long int") == 0) {
      __format_numeric(result, print_prefix, "%ld");
    } else if (strcmp(type, "unsigned long int") == 0 || //
               strcmp(type, "long unsigned int") == 0 || //
               strcmp(type, "unsigned long") == 0) {
      __format_numeric(result, print_prefix, "%lu");
    } else if (strcmp(type, "long long") == 0 || //
               strcmp(type, "long long int") == 0) {
      __format_numeric(result, print_prefix, "%lld");
    } else if (strcmp(type, "unsigned long long") == 0 || //
               strcmp(type, "unsigned long long int") == 0) {
      __format_numeric(result, print_prefix, "%llu");
    } else if (strcmp(type, "off_t") == 0) {
      __format_numeric(result, print_prefix, "%jd");
    } else if (strcmp(type, "goffset") == 0) {
      result->format = "%\"G_GOFFSET_FORMAT\"";
    } else if (strcmp(type, "size_t") == 0) {
      __format_numeric(result, print_prefix, "%zu");
    } else if (strcmp(type, "gsize") == 0) {
      result->format = "%\"G_GSIZE_FORMAT\"";
    } else if (strcmp(type, "ssize_t") == 0) {
      __format_numeric(result, print_prefix, "%zd");
    } else if (strcmp(type, "gssize") == 0) {
      result->format = "%\"G_GSSIZE_FORMAT\"";
    } else if (strcmp(type, "int64_t") == 0) {
      result->format = "%\"PRId64\"";
    } else if (strcmp(type, "uint64_t") == 0) {
      result->format = "%\"PRIu64\"";
    } else if (strcmp(type, "guint64") == 0) {
      result->format = "%\"G_GUINT64_FORMAT\"";
    } else if (strcmp(type, "uintptr_t") == 0) {
      result->format = "%\"PRIuPTR\"";
    } else if (strcmp(type, "guintptr") == 0) {
      result->format = "%\"G_GUINTPTR_FORMAT\"";
    } else if (strcmp(type, "iintptr_t") == 0) {
      result->format = "%\"PRIiPTR\"";
    } else if (strcmp(type, "gintptr") == 0) {
      result->format = "%\"G_GINTPTR_FORMAT\"";
    } else if (strcmp(type, "float") == 0 || //
               strcmp(type, "gfloat") == 0 || //
               strcmp(type, "double") == 0 || //
               strcmp(type, "gdouble") == 0) {
      __format_numeric(result, print_prefix, "%f");
    } else if (strcmp(type, "long double") == 0) {
      __format_numeric(result, print_prefix, "%Lf");
    } else {
      if (strchr(type, ' ') == NULL) {
        const char *prefix = "&";
        sp_str buf_tmp;
        sp_str_init(&buf_tmp, 0);

        /* Example: type_t */
        result->format = "%s";
        if (result->pointer) {
          prefix = "";
        }
        sp_str_appends(&buf_tmp, "sp_debug_", type, "(", prefix, print_prefix,
                       result->variable, ")", NULL);
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
  char *type              = NULL;

  result = __field_name(ctx, subject, "identifier");
  if (result) {
    type = __field_type(ctx, subject, result, "");
    /* printf("%s: %s\n", type, result->variable); */
    __format(ctx, result, type, "");
  }

  if (result && result->format && result->variable) {
    result->complete = true;
  }

  free(type);
  return result;
}

static int
sp_print_function(struct sp_ts_Context *ctx, TSNode subject)
{
  int res = EXIT_SUCCESS;
  TSNode tmp;
  struct arg_list field_dummy = {0};
  struct arg_list *field_it   = &field_dummy;
  size_t complete             = 0;
  sp_str buf;
  sp_str_init(&buf, 0);

  /* printf("here!"); */
  tmp = sp_find_direct_child_by_type(subject, "function_declarator");
  if (!ts_node_is_null(tmp)) {
    tmp = sp_find_direct_child_by_type(tmp, "parameter_list");
    if (!ts_node_is_null(tmp)) {
      uint32_t i;
      struct arg_list *arg = NULL;

      for (i = 0; i < ts_node_child_count(tmp); ++i) {
        TSNode parameter_decl = ts_node_child(tmp, i);
        if (strcmp(ts_node_type(parameter_decl), "parameter_declaration") ==
            0) {
#if 0
  uint32_t a;
  for (a = 0; a < ts_node_child_count(parameter_decl); ++a) {
    TSNode child = ts_node_child(parameter_decl, a);
    printf("- %s\n", ts_node_string(child));
  }
#endif
          if ((arg = __parameter_to_arg(ctx, parameter_decl))) {
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

  if (ctx->domain == DEFAULT_DOMAIN) {
    sp_str_append(&buf, "  printf(");
  } else if (ctx->domain == SYSLOG_DOMAIN) {
    sp_str_append(&buf, "  syslog(LOG_ERR,");
  } else if (ctx->domain == LINUX_KERNEL_DOMAIN) {
    sp_str_append(&buf, "  printk(KERN_ERR ");
  }
  sp_str_append(&buf, "\"%s:");
  field_it = field_dummy.next;
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
  sp_str_append(&buf, "\\n\", __func__");
  field_it = field_dummy.next;
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
  sp_str_free(&buf);

  return res;
}

static struct arg_list *
__field_to_arg(struct sp_ts_Context *ctx, TSNode subject)
{
  struct arg_list *result = NULL;
  char *type              = NULL;

  /* fprintf(stderr, "%s\n", __func__); */

  /* TODO result->format, result->variable strdup() */

  if ((result = __field_name(ctx, subject, "field_identifier"))) {
    /* fprintf(stderr,"%s\n", result->variable); */
    type = __field_type(ctx, subject, result, "in->");
    /* fprintf(stderr, "type[%s]\n", type); */
    __format(ctx, result, type, "in->");
  }

  if (result && result->format && result->variable) {
    result->complete = true;
  }

  free(type);
  return result;
}

static int
sp_print_struct(struct sp_ts_Context *ctx, TSNode subject)
{
  char *type_name             = NULL;
  bool type_name_t            = false;
  size_t complete             = 0;
  struct arg_list field_dummy = {0};
  struct arg_list *field_it   = &field_dummy;
  uint32_t i;
  TSNode tmp;
  sp_str buf;

  sp_str_init(&buf, 0);

#if 0
  fprintf(stderr, "%s\n", __func__);
  fprintf(stderr,"--\n");
  for (i = 0; i < ts_node_child_count(subject); ++i) {
    TSNode child = ts_node_child(subject, i);
    uint32_t s   = ts_node_start_byte(child);
    uint32_t e   = ts_node_end_byte(child);
    uint32_t len = e - s;
    fprintf(stderr, ".%u\n", i);
    fprintf(stderr, "children: %u\n", ts_node_child_count(child));
    fprintf(stderr, "%.*s: %s\n", (int)len, &ctx->file.content[s],
            ts_node_type(child));
  }
  fprintf(stderr,"--\n");
#endif
#if 0
  char *p = ts_node_string(subject);
  fprintf(stdout, "%s\n", p);
  free(p);
#endif

  tmp = sp_find_direct_child_by_type(subject, "type_identifier");
  if (!ts_node_is_null(tmp)) {
    /* struct type_name { ... }; */
    type_name = sp_struct_value(ctx, tmp);
  } else {
    TSNode parent = ts_node_parent(subject);
    if (!ts_node_is_null(parent)) {
      if (strcmp(ts_node_type(parent), "type_definition") == 0) {
        tmp = sp_find_direct_child_by_type(parent, "type_identifier");
        if (!ts_node_is_null(tmp)) {
          /* typedef struct * { ... } type_name; */
          type_name_t = true;
          type_name   = sp_struct_value(ctx, tmp);
        }
      }
    }
  }

  tmp = sp_find_direct_child_by_type(subject, "field_declaration_list");
  if (!ts_node_is_null(tmp)) {
    for (i = 0; i < ts_node_child_count(tmp); ++i) {
      TSNode field = ts_node_child(tmp, i);
      /* fprintf(stderr, "i.%u\n", i); */
      if (strcmp(ts_node_type(field), "field_declaration") == 0) {
        struct arg_list *arg = NULL;

        if ((arg = __field_to_arg(ctx, field))) {
          field_it = field_it->next = arg;
        }

#if 0
        uint32_t a;
        for (a = 0; a < ts_node_child_count(field); ++a) {
          TSNode child = ts_node_child(field, a);
          uint32_t s   = ts_node_start_byte(child);
          uint32_t e   = ts_node_end_byte(child);
          uint32_t len = e - s;
          fprintf(stderr, ".%d\n", a);
          fprintf(stderr, "children: %u\n", ts_node_child_count(child));
          fprintf(stderr, "%.*s: %s\n", (int)len, &ctx->file.content[s],
                  ts_node_type(child));
        }
#endif
      }
    }
  }

#if 0
  for (i = 0; i < ts_node_child_count(subject); ++i) {
    uint32_t s   = ts_node_start_byte(ts_node_child(subject, i));
    uint32_t e   = ts_node_end_byte(ts_node_child(subject, i));
    uint32_t len = e - s;
    fprintf(stderr, ".%u\n", i);
    fprintf(stderr, "children: %u\n", ts_node_child_count(subject));
    fprintf(stderr, "%.*s: %s\n", (int)len, &ctx->file.content[s],
            ts_node_type(ts_node_child(subject, i)));
  }
#endif

  sp_str_appends(&buf, "static inline const char* sp_debug_", type_name, "(",
                 NULL);
  sp_str_appends(&buf, "const ", type_name_t ? "" : "struct ", type_name,
                 " *in) {\n", NULL);
  sp_str_append(&buf, "  static char buf[256] = {'\\0'};\n");
  sp_str_appends(&buf, "  if (!in) return \"", type_name, "(NULL)\";\n", NULL);
  field_it = field_dummy.next;
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
  sp_str_append(&buf, "}\", in");

  field_it = field_dummy.next;
  while (field_it) {
    if (field_it->complete) {
      sp_str_append(&buf, ", ");
      if (field_it->complex_printf) {
        /* char buf_tmp[256] = {'\0'}; */
        assert(field_it->complex_raw);
        //TODO support "in->{var}".format(map) expansion
        /*         snprintf(buf_tmp, sizeof(buf_tmp), field_it->complex_raw, "in", "in", */
        /*                  "in", "in"); */
        /* sp_str_append(&buf, buf_tmp); */
        sp_str_append(&buf, field_it->complex_raw);
      } else {
        sp_str_appends(&buf, "in->", field_it->variable, NULL);
      }
    }
    field_it = field_it->next;
  } //while
  sp_str_append(&buf, ");\n");
  sp_str_append(&buf, "  return buf;\n");
  sp_str_append(&buf, "}\n");

  fprintf(stdout, "%s", sp_str_c_str(&buf));

  free(type_name);
  sp_str_free(&buf);
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

  body = sp_find_direct_child_by_type(subject, "compound_statement");
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
          TSNode tmp = sp_find_direct_child_by_type(root, "declaration");
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
          const char *struct_spec   = "struct_specifier";
          const char *class_spec    = "class_specifier";
          const char *enum_spec     = "enum_specifier";
          const char *function_spec = "function_definition";
          TSNode found = sp_find_parent(highligted, struct_spec, enum_spec,
                                        function_spec, class_spec);
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
            } else if (strcmp(ts_node_type(found), function_spec) == 0) {
              if (strcmp(in_type, "crunch") == 0) {
                /* printf("%s\n", ts_node_string(found)); */
                res = sp_print_function(&ctx, found);
              } else {
                uint32_t line;
                line = sp_find_open_bracket(found);
                fprintf(stdout, "%u", line);
                res = EXIT_SUCCESS;
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
//TODO print idincation before return
//TODO maybe if there is no struct prefix we print %p (example: struct IOPort vs IOPort)

//TODO when we make assumption example (unsigned char*xxx, size_t l_xxx) make a comment in the debug function
// example: NOTE: assumes xxx and l_xxx is related

// TODO generate 2 print function typedef struct name {} name_t;

// TODO ignore:
// spinlock_t		lock; /* lock for the whole structure */
// struct mutex		io_mutex;
//
// TODO if in LINUX_KERNEL_DOMAIN we can print function %pF
