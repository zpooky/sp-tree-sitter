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

struct sp_ts_Context {
  struct sp_ts_file file;
  TSTree *tree;
};

extern const TSLanguage *
tree_sitter_c(void);

static TSNode
sp_find_parent(TSNode subject, const char *needle0, const char *needle1)
{
  TSNode it          = subject;
  const TSNode empty = {0};

  while (!ts_node_is_null(it)) {
    if (strcmp(ts_node_type(it), needle0) == 0 ||
        strcmp(ts_node_type(it), needle1) == 0) {
      return it;
    }
    it = ts_node_parent(it);
  }
  return empty;
}
static TSNode
sp_find_direct_child(TSNode subject, const char *needle)
{
  TSNode empty = {0};
  uint32_t i;
  for (i = 0; i < ts_node_child_count(subject); ++i) {
    TSNode child = ts_node_child(subject, i);
    if (strcmp(ts_node_type(child), needle) == 0) {
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

  tmp = sp_find_direct_child(subject, "type_identifier");
  if (!ts_node_is_null(tmp)) {
    /* struct type_name { ... }; */
    type_name = sp_struct_value(ctx, tmp);
  } else {
    TSNode parent = ts_node_parent(subject);
    if (!ts_node_is_null(parent)) {
      if (strcmp(ts_node_type(parent), "type_definition") == 0) {
        tmp = sp_find_direct_child(parent, "type_identifier");
        if (!ts_node_is_null(tmp)) {
          /* typedef struct * { ... } type_name; */
          type_name_t = true;
          type_name   = sp_struct_value(ctx, tmp);
        }
      }
    }
  }

  tmp = sp_find_direct_child(subject, "enumerator_list");
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

  sp_str_append(&buf, "static inline const char* sp_print_");
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

static struct arg_list *
__sp_to_str_struct_field(struct sp_ts_Context *ctx, TSNode subject)
{
  struct arg_list *result = NULL;
  uint32_t pointer        = 0;
  TSNode tmp;
  char *type = NULL;
  bool array = false;

  fprintf(stderr, "%s\n", __func__);

  result = calloc(1, sizeof(*result));

  /* TODO result->format, result->variable strdup() */

  tmp = sp_find_direct_child(subject, "field_identifier");
  if (!ts_node_is_null(tmp)) {
    result->variable = sp_struct_value(ctx, tmp);
  } else {
    tmp = sp_find_direct_child(subject, "pointer_declarator");
    if (!ts_node_is_null(tmp)) {
      tmp = __rec_search(ctx, tmp, "field_identifier", 1, &pointer);
      if (!ts_node_is_null(tmp)) {
        result->variable = sp_struct_value(ctx, tmp);
      }

#if 0
      uint32_t i;
      for (i = 0; i < ts_node_child_count(tmp); ++i) {
        TSNode child = ts_node_child(tmp, i);
        uint32_t s   = ts_node_start_byte(child);
        uint32_t e   = ts_node_end_byte(child);
        uint32_t len = e - s;
        fprintf(stderr, ".%u\n", i);
        fprintf(stderr, "children: %u\n", ts_node_child_count(child));
        fprintf(stderr, "%.*s: %s\n", (int)len, &ctx->file.content[s],
                ts_node_type(child));
      }
#endif
    } else {
      tmp = sp_find_direct_child(subject, "array_declarator");
      if (!ts_node_is_null(tmp)) {
#if 0
        fprintf(stderr,"--\n");
        uint32_t i;
        for (i = 0; i < ts_node_child_count(tmp); ++i) {
          TSNode child = ts_node_child(tmp, i);
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
        TSNode field_id = sp_find_direct_child(subject, "field_identifier");
        if (!ts_node_is_null(field_id)) {
          array            = true;
          result->variable = sp_struct_value(ctx, field_id);
          /* TODO: '[' XXX ']' */
        }
      }
    }
  }

  tmp = sp_find_direct_child(subject, "primitive_type");
  if (!ts_node_is_null(tmp)) {
    /* $primitive_type $field_identifier; */
    type = sp_struct_value(ctx, tmp);
  } else {
    tmp = sp_find_direct_child(subject, "sized_type_specifier");
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
      tmp = sp_find_direct_child(subject, "type_identifier");
      if (!ts_node_is_null(tmp)) {
        /* $type_identifier $field_identifier;
         * Example:
         *  type_t type0;
         *  gint int0;
         */
        type = sp_struct_value(ctx, tmp);
      } else {
        tmp = sp_find_direct_child(subject, "enum_specifier");
        if (!ts_node_is_null(tmp)) {

          tmp = sp_find_direct_child(tmp, "type_identifier");
          if (!ts_node_is_null(tmp)) {
            result->format = "%s";
            if (pointer > 1) {
            } else {
              char *enum_type    = sp_struct_value(ctx, tmp);
              const char *prefix = "&";
              sp_str buf_tmp;
              sp_str_init(&buf_tmp, 0);

              /* Example: enum type_t */
              if (pointer) {
                prefix = "";
              }
              sp_str_appends(&buf_tmp, "sp_print_", enum_type, "(", prefix,
                             "%s->", result->variable, ")", NULL);
              result->complex_raw    = strdup(sp_str_c_str(&buf_tmp));
              result->complex_printf = true;

              /* enum $type_identifier $field_identifier; */
              // sp_print_$enum_type($var->($result->variable));
              sp_str_free(&buf_tmp);
              free(enum_type);
            }
          }
        } else {
          tmp = sp_find_direct_child(subject, "struct_specifier");
          if (!ts_node_is_null(tmp)) {

            tmp = sp_find_direct_child(tmp, "type_identifier");
            if (!ts_node_is_null(tmp)) {
              result->format = "%s";
              if (pointer > 1) {
              } else {
                char *struct_type  = sp_struct_value(ctx, tmp);
                const char *prefix = "&";
                sp_str buf_tmp;
                sp_str_init(&buf_tmp, 0);

                /* Example: struct type_t */
                if (pointer) {
                  prefix = "";
                }
                sp_str_appends(&buf_tmp, "sp_print_", struct_type, "(", prefix,
                               "%s->", result->variable, ")", NULL);
                result->complex_raw    = strdup(sp_str_c_str(&buf_tmp));
                result->complex_printf = true;

                /* struct $struct_specifier $field_identifier; */
                // sp_print_$struct_type($var->($result->variable));
                sp_str_free(&buf_tmp);
                free(struct_type);
              }
            }
          }
        }
      }
    }
  }

  /* https://developer.gnome.org/glib/stable/glib-Basic-Types.html */
  /* https://en.cppreference.com/w/cpp/types/integer */
  //TODO strdup
  if (type) {
    if (pointer > 1) {
      result->format = "%p";
    } else if (strcmp(type, "gboolean") == 0 || //
               strcmp(type, "bool") == 0 || //
               strcmp(type, "boolean") == 0) {
      sp_str buf_tmp;
      sp_str_init(&buf_tmp, 0);
      result->format = "%s";

      if (pointer) {
        sp_str_appends(&buf_tmp, "!%s->", result->variable,
                       " ? \"NULL\" : *%s->", result->variable, NULL);
      } else {
        sp_str_appends(&buf_tmp, "%s->", result->variable, NULL);
      }
      sp_str_appends(&buf_tmp, " ? \"TRUE\" : \"FALSE\"", NULL);
      result->complex_raw    = strdup(sp_str_c_str(&buf_tmp));
      result->complex_printf = true;

      sp_str_free(&buf_tmp);
    } else if (strcmp(type, "char") == 0 || //
               strcmp(type, "gchar") == 0 || //
               strcmp(type, "gint8") == 0 || //
               strcmp(type, "int8_t") == 0) {
      if (array) {
        result->format = "%.s";
      } else if (pointer) {
        result->format = "%s";
      } else {
        result->format = "%c";
      }
    } else if (strcmp(type, "uchar") == 0 || //
               strcmp(type, "guchar") == 0 || //
               strcmp(type, "guint8") == 0 || //
               strcmp(type, "uint8_t") == 0) {
      result->format = "%d";
      /* TODO if pointer hex? */
    } else if (strcmp(type, "short") == 0 || //
               strcmp(type, "gshort") == 0 || //
               strcmp(type, "gint16") == 0 || //
               strcmp(type, "int16_t") == 0) {
      result->format = "%d";
    } else if (strcmp(type, "unsigned short") == 0 || //
               strcmp(type, "gushort") == 0 || //
               strcmp(type, "guint16") == 0 || //
               strcmp(type, "uint16_t") == 0) {
      result->format = "%u";
    } else if (strcmp(type, "int") == 0 || //
               strcmp(type, "signed int") == 0 || //
               strcmp(type, "gint") == 0 || //
               strcmp(type, "gint32") == 0 || //
               strcmp(type, "int32_t") == 0) {
      result->format = "%d";
    } else if (strcmp(type, "unsigned") == 0 || //
               strcmp(type, "unsigned int") == 0 || //
               strcmp(type, "guint") == 0 || //
               strcmp(type, "guint32") == 0 || //
               strcmp(type, "uint32_t") == 0) {
      result->format = "%u";
    } else if (strcmp(type, "long") == 0 || //
               strcmp(type, "long int") == 0) {
      result->format = "%ld";
    } else if (strcmp(type, "unsigned long int") == 0 || //
               strcmp(type, "long unsigned int") == 0 || //
               strcmp(type, "unsigned long") == 0) {
      result->format = "%lu";
    } else if (strcmp(type, "long long") == 0 || //
               strcmp(type, "long long int") == 0) {
      result->format = "%lld";
    } else if (strcmp(type, "unsigned long long") == 0 || //
               strcmp(type, "unsigned long long int") == 0) {
      result->format = "%llu";
    } else if (strcmp(type, "off_t") == 0) {
      result->format = "%jd";
    } else if (strcmp(type, "goffset") == 0) {
      result->format = "\"%\"G_GOFFSET_FORMAT";
    } else if (strcmp(type, "size_t") == 0) {
      result->format = "%zu";
    } else if (strcmp(type, "gsize") == 0) {
      result->format = "\"%\"G_GSIZE_FORMAT";
    } else if (strcmp(type, "ssize_t") == 0) {
      result->format = "%zd";
    } else if (strcmp(type, "gssize") == 0) {
      result->format = "\"%\"G_GSSIZE_FORMAT";
    } else if (strcmp(type, "int64_t") == 0) {
      result->format = "\"%\"PRId64";
    } else if (strcmp(type, "uint64_t") == 0) {
      result->format = "\"%\"PRIu64";
    } else if (strcmp(type, "guint64") == 0) {
      result->format = "\"%\"G_GUINT64_FORMAT";
    } else if (strcmp(type, "uintptr_t") == 0) {
      result->format = "\"%\"PRIuPTR";
    } else if (strcmp(type, "guintptr") == 0) {
      result->format = "\"%\"G_GUINTPTR_FORMAT";
    } else if (strcmp(type, "iintptr_t") == 0) {
      result->format = "\"%\"PRIiPTR";
    } else if (strcmp(type, "gintptr") == 0) {
      result->format = "\"%\"G_GINTPTR_FORMAT";
    } else if (strcmp(type, "float") == 0 || //
               strcmp(type, "gfloat") == 0 || //
               strcmp(type, "double") == 0 || //
               strcmp(type, "gdouble") == 0) {
      result->format = "%f";
    } else if (strcmp(type, "long double") == 0) {
      result->format = "%Lf";
    } else {
      if (strchr(type, ' ') == NULL) {
        const char *prefix = "&";
        sp_str buf_tmp;
        sp_str_init(&buf_tmp, 0);

        /* Example: type_t */
        result->format = "%s";
        if (pointer) {
          prefix = "";
        }
        sp_str_appends(&buf_tmp, "sp_print_", type, "(", prefix, "%s->",
                       result->variable, ")", NULL);
        result->complex_raw    = strdup(sp_str_c_str(&buf_tmp));
        result->complex_printf = true;

        sp_str_free(&buf_tmp);
      } else {
        assert(false);
      }
    }
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

  fprintf(stderr, "%s\n", __func__);
#if 0
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

  tmp = sp_find_direct_child(subject, "type_identifier");
  if (!ts_node_is_null(tmp)) {
    /* struct type_name { ... }; */
    type_name = sp_struct_value(ctx, tmp);
  } else {
    TSNode parent = ts_node_parent(subject);
    if (!ts_node_is_null(parent)) {
      if (strcmp(ts_node_type(parent), "type_definition") == 0) {
        tmp = sp_find_direct_child(parent, "type_identifier");
        if (!ts_node_is_null(tmp)) {
          /* typedef struct * { ... } type_name; */
          type_name_t = true;
          type_name   = sp_struct_value(ctx, tmp);
        }
      }
    }
  }

  tmp = sp_find_direct_child(subject, "field_declaration_list");
  if (!ts_node_is_null(tmp)) {
    for (i = 0; i < ts_node_child_count(tmp); ++i) {
      TSNode field = ts_node_child(tmp, i);
      /* fprintf(stderr, "i.%u\n", i); */
      if (strcmp(ts_node_type(field), "field_declaration") == 0) {
        struct arg_list *arg = NULL;
        uint32_t a;

        if ((arg = __sp_to_str_struct_field(ctx, field))) {
          field_it = field_it->next = arg;
        }

#if 1
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
  field_it = field_dummy.next;
  while (field_it) {
    fprintf(stderr, "%s, %s\n", field_it->format, field_it->variable);
    field_it = field_it->next;
  }

  sp_str_appends(&buf, "static inline const char* sp_print_", type_name, "(",
                 NULL);
  sp_str_appends(&buf, "const ", type_name_t ? "" : "struct ", type_name,
                 " *in) {\n", NULL);
  sp_str_append(&buf, "  static char buf[256] = {'\\0'};\n");
  sp_str_append(&buf, "  if (!in) return \"NULL\";\n");
  field_it = field_dummy.next;
  sp_str_appends(&buf, "  snprintf(buf, sizeof(buf), \"", type_name, "(%p)[",
                 NULL);
  while (field_it) {
    if (field_it->complete) {
      sp_str_appends(&buf, field_it->variable, "[", field_it->format, "]",
                     NULL);
      ++complete;
    } else {
      fprintf(stderr, "Unknown: %s\n",
              field_it->variable ? field_it->variable : "NULL");
    }
    field_it = field_it->next;
  } //while
  sp_str_append(&buf, "]\", in");

  field_it = field_dummy.next;
  while (field_it) {
    if (field_it->complete) {
      sp_str_append(&buf, ", ");
      if (field_it->complex_printf) {
        char buf_tmp[256] = {'\0'};
        //TODO support "in->{var}".format(map) expansion
        snprintf(buf_tmp, sizeof(buf_tmp), field_it->complex_raw, "in", "in",
                 "in", "in");
        sp_str_append(&buf, buf_tmp);
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
    fprintf(stderr, "%s crunch|line file line column\n", argv[0]);
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
    TSParser *parser       = ts_parser_new();
    const TSLanguage *lang = tree_sitter_c();
    ts_parser_set_language(parser, lang);

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
          TSNode tmp = sp_find_direct_child(root, "declaration");
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
          const char *struct_spec = "struct_specifier";
          const char *enum_spec   = "enum_specifier";
          TSNode found = sp_find_parent(highligted, struct_spec, enum_spec);

          if (!ts_node_is_null(found)) {
            if (strcmp(ts_node_type(found), struct_spec) == 0) {
              if (strcmp(in_type, "crunch") == 0) {
                res = sp_print_struct(&ctx, found);
              } else {
                uint32_t last_line;
                last_line = sp_find_last_line(found);
                fprintf(stdout, "%u", last_line);
                res = EXIT_SUCCESS;
              }
            } else if (strcmp(ts_node_type(found), enum_spec) == 0) {
              if (strcmp(in_type, "crunch") == 0) {
                res = sp_print_enum(&ctx, found);
              } else {
                uint32_t last_line;
                last_line = sp_find_last_line(found);
                fprintf(stdout, "%u", last_line);
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
