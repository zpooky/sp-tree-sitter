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
#include <limits.h>
#include <errno.h>
#include <jansson.h>

enum sp_ts_SourceDomain {
  DEFAULT_DOMAIN = 0,
  LOG_ERR_DOMAIN,
  SYSLOG_DOMAIN,
  LINUX_KERNEL_DOMAIN,
  F_ERROR_DOMAIN,
};

struct sp_ts_Context {
  struct sp_ts_file file;
  TSTree *tree;
  enum sp_ts_SourceDomain domain;
  uint32_t output_line;
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

static char *
node_value(struct sp_ts_Context *ctx, TSNode node)
{
  uint32_t s   = ts_node_start_byte(node);
  uint32_t e   = ts_node_end_byte(node);
  uint32_t len = e - s;
  char *it     = &ctx->file.content[s];
  //trim
  while (len) {
    if (*it == ' ' || *it == '\n' || *it == '\t') {
      ++it;
      --len;
    } else {
      break;
    }
  }
  while (len) {
    char c = it[len - 1];
    if (c == ' ' || c == '\n' || c == '\t') {
      --len;
    } else {
      break;
    }
  }
  return strndup(it, len);
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
  if (strcasestr(file, "/linux-axis") != NULL)
    return LINUX_KERNEL_DOMAIN;
  if (strcasestr(file, "/modartpec") != NULL)
    return LINUX_KERNEL_DOMAIN;
  if (strcasestr(file, "/workspace/sources/ioboxd") != NULL)
    return LOG_ERR_DOMAIN;
  if (strcasestr(file, "/eventbridge-plugins-propertychanged") != NULL)
    return F_ERROR_DOMAIN;
  if (strcasestr(file, "/dists/") != NULL)
    return SYSLOG_DOMAIN;

  return DEFAULT_DOMAIN;
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

static int32_t
find_direct_chld_index_by_type(TSNode subject, const char *needle)
{
  uint32_t i;
  for (i = 0; i < ts_node_child_count(subject); ++i) {
    TSNode child          = ts_node_child(subject, i);
    const char *node_type = ts_node_type(child);
    if (strcmp(node_type, needle) == 0) {
      return (int32_t)i;
    }
  }
  return -1;
}

static TSNode
find_direct_chld_by_type(TSNode subject, const char *needle)
{
  TSNode empty  = {0};
  int32_t index = find_direct_chld_index_by_type(subject, needle);
  if (index >= 0) {
    return ts_node_child(subject, (uint32_t)index);
  }

  return empty;
}

static TSNode
find_rec_chld_by_type(TSNode subject, const char *needle)
{
  TSNode empty = {0};
  uint32_t i;
  for (i = 0; i < ts_node_child_count(subject); ++i) {
    TSNode child = ts_node_child(subject, i);
    TSNode tmp   = find_rec_chld_by_type(child, needle);
    if (!ts_node_is_null(tmp)) {
      return tmp;
    }
    const char *node_type = ts_node_type(child);
    if (strcmp(node_type, needle) == 0) {
      return child;
    }
  }
  return empty;
}

struct sp_str_list;
struct sp_str_list {
  char *value;
  struct sp_str_list *next;
};

static bool
parse_int(struct sp_ts_Context *ctx, TSNode subject, int64_t *result)
{
  char buffer[64] = {'\0'};
  uint32_t s      = ts_node_start_byte(subject);
  uint32_t e      = ts_node_end_byte(subject);
  int len         = (int)(e - s);
  char *str_end   = NULL;

  sprintf(buffer, "%.*s", len, &ctx->file.content[s]);
  *result = strtoll(buffer, &str_end, 10);

  if (errno == ERANGE && (*result == INT64_MAX || *result == INT64_MIN)) {
    return false;
  } else if (*result == 0 && errno != 0) {
    return false;
  } else if (str_end == buffer) {
    return false;
  }

  return true;
}

static bool
is_enum_bitmask(struct sp_ts_Context *ctx, TSNode subject)
{
  TSNode enum_list;
#define MAX_LITERALS 200
  int64_t literals[MAX_LITERALS] = {0};
  size_t n_literals              = 0;

  debug_subtypes_rec(ctx, subject, 0);
  enum_list = find_direct_chld_by_type(subject, "enumerator_list");
  if (!ts_node_is_null(enum_list)) {
    char *enum_cache[MAX_LITERALS] = {NULL};
    size_t n_enum_cache            = 0;
    uint32_t i;

    for (i = 0; i < ts_node_child_count(enum_list); ++i) {
      TSNode enumerator = ts_node_child(enum_list, i);
      if (strcmp(ts_node_type(enumerator), "enumerator") == 0) {
        TSNode id = ts_node_child(enumerator, 0);
        //TODO reclaim
        enum_cache[n_enum_cache++] = sp_struct_value(ctx, id);
      }
    }

    for (i = 0; i < ts_node_child_count(enum_list); ++i) {
      TSNode enumerator = ts_node_child(enum_list, i);
      if (strcmp(ts_node_type(enumerator), "enumerator") == 0) {
        TSNode tmp;
        tmp = find_direct_chld_by_type(enumerator, "parenthesized_expression");
        if (!ts_node_is_null(tmp)) {
          /* [parenthesized_expression]
           *   [(]
           *   [binary_expression]
           *     [number_literal]: 1
           *     [<<]
           *     [number_literal]: 1
           *   [)]
           *
           * [parenthesized_expression]
           *   [(]
           *   [binary_expression]
           *     [identifier]: G_PARAM_READABLE
           *     [|]
           *     [identifier]: G_PARAM_WRITABLE
           *   [)]
           */
          TSNode node0;
          TSNode op;
          TSNode node1;

          if (ts_node_child_count(tmp) != 3) {
            return false;
          }

          node0 = ts_node_child(tmp, 0);
          if (strcmp(ts_node_type(node0), "(") != 0) {
            return false;
          }

          op = ts_node_child(tmp, 1);
          if (strcmp(ts_node_type(op), "binary_expression") != 0) {
            return false;
          }
          node1 = ts_node_child(tmp, 2);
          if (strcmp(ts_node_type(node1), ")") != 0) {
            return false;
          }
          enumerator = tmp;
        }
        tmp = find_direct_chld_by_type(enumerator, "binary_expression");
        if (!ts_node_is_null(tmp)) {
          /* [binary_expression]
             *   [number_literal]: 1
             *   [<<]
             *   [number_literal]: 30
             *
             * [binary_expression]
             *   [binary_expression]
             *     [identifier]: G_PARAM_READABLE
             *     [|]
             *     [identifier]: G_PARAM_WRITABLE
             *   [|]
             *   [identifier]: G_PARAM_CONSTRUCT
             */
          TSNode node0;
          TSNode op;
          TSNode node1;
          if (ts_node_child_count(tmp) != 3) {
            return false;
          }

          node0 = ts_node_child(tmp, 0);
          op    = ts_node_child(tmp, 1);
          node1 = ts_node_child(tmp, 2);

          /* printf("%s\n", ts_node_type(op)); */
          if (strcmp(ts_node_type(op), "<<") == 0) {
            uint32_t a;
            int64_t literal0;
            int64_t literal1;
            int64_t tmp_mask = 0;
            if (!parse_int(ctx, node0, &literal0)) {
              return false;
            }
            if (!parse_int(ctx, node1, &literal1)) {
              return false;
            }
            literals[n_literals++] = literal0 << literal1;
            for (a = 0; a < n_literals; ++a) {
              if (tmp_mask & literals[a]) {
                return false;
              }
              tmp_mask |= literals[a];
            }
          } else if (strcmp(ts_node_type(op), "|") == 0) {
            //TODO
          } else {
            return false;
          }
        } else {
          tmp = find_direct_chld_by_type(enumerator, "number_literal");
          if (!ts_node_is_null(tmp)) {
            uint32_t a;
            /* [number_literal]: 1 */
            int64_t literal;
            int64_t tmp_mask = 0;
            if (!parse_int(ctx, tmp, &literal)) {
              return false;
            }
            literals[n_literals++] = literal;
            for (a = 0; a < n_literals; ++a) {
              if (tmp_mask & literals[a]) {
                return false;
              }
              tmp_mask |= literals[a];
            }

          } else {
            tmp = find_direct_chld_by_type(enumerator, "call_expression");
            if (!ts_node_is_null(tmp)) {
              /* [call_expression]
                 *   [parenthesized_expression]
                 *     [(]
                 *     [identifier]: gint
                 *     [)]
                 *   [argument_list]
                 *     [(]
                 *     [binary_expression]
                 *       [number_literal]: 1u
                 *       [<<]
                 *       [number_literal]: 31
                 *     [)]
                 */
              /* TODO */
            } else {
              /* [identifier]: G_PARAM_PRIVATE
               * [=]
               * [identifier]: G_PARAM_STATIC_NAME
               */
              TSNode op;
              TSNode node1;
              if (ts_node_child_count(enumerator) != 3) {
                return false;
              }

              op = ts_node_child(enumerator, 1);
              if (strcmp(ts_node_type(op), "=") != 0) {
                return false;
              }
              node1 = ts_node_child(enumerator, 2);
              if (strcmp(ts_node_type(node1), "identifier") != 0) {
                return false;
              } else {
                size_t a;
                char *ref  = node_value(ctx, node1);
                bool found = false;
                for (a = 0; a < n_enum_cache; ++a) {
                  if (strcmp(enum_cache[a], ref) == 0) {
                    found = true;
                    break;
                  }
                }
                free(ref);
                if (!found) {
                  return false;
                }
              }
            }
          }
        }
      }
    } //for
  }

  return true;
}

static void
print_json_response(uint32_t line, const char *data)
{
  json_t *root = json_object();
  {
    json_t *json_inserts = json_array();
    {
      json_t *json_insert = json_object();
      json_object_set_new(json_insert, "data", json_string(data));
      json_object_set_new(json_insert, "line", json_integer(line));
      json_array_append_new(json_inserts, json_insert);
    }
    json_object_set_new(root, "inserts", json_inserts);
  }

  {
    char *json_response;
    json_response = json_dumps(root, JSON_PRESERVE_ORDER);
    fprintf(stdout, "%s", json_response);
    fflush(stdout);
    free(json_response);
  }

  json_decref(root);
}

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
  if (is_enum_bitmask(ctx, subject)) {
    sp_str_append(&buf, "  static char buf[1024] = {'\\0'};\n");
    sp_str_append(&buf, "  if (!in) return \"NULL\";\n");
    enums_it = dummy.next;
    for (; enums_it; enums_it = enums_it->next) {
      sp_str_appends(&buf, "  if (*in & ", enums_it->value, ") ", NULL);
      sp_str_appends(&buf, "strcat(buf, \"|", enums_it->value, "\");\n", NULL);
    }
    sp_str_append(&buf, "  return buf;\n");
  } else {
    sp_str_append(&buf, "  if (!in) return \"NULL\";\n");
    sp_str_append(&buf, "  switch (*in) {\n");
    enums_it = dummy.next;
    for (; enums_it; enums_it = enums_it->next) {
      sp_str_append(&buf, "    case ");
      if (enum_class) {
        sp_str_appends(&buf, type_name, "::", NULL);
      }
      sp_str_append(&buf, enums_it->value);
      sp_str_append(&buf, ": return \"");
      sp_str_append(&buf, enums_it->value);
      sp_str_append(&buf, "\";\n");
    }

    sp_str_append(&buf, "    default: return \"__UNDEF\";\n");
    sp_str_append(&buf, "  }\n");
  }
  sp_str_append(&buf, "}\n");

  /* fprintf(stdout, "%s", sp_str_c_str(&buf)); */
  print_json_response(ctx->output_line, sp_str_c_str(&buf));

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
  /* fprintf(stderr, "%s:=========================\n", __func__); */

  tmp = find_direct_chld_by_type(subject, "primitive_type");
  if (!ts_node_is_null(tmp)) {
    /* $primitive_type $field_identifier; */
    type = sp_struct_value(ctx, tmp);
    /* fprintf(stderr, "%s:1 [%s]\n", __func__, type); */
  } else {
    tmp = find_direct_chld_by_type(subject, "sized_type_specifier");
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
      /* fprintf(stderr, "%s:2 [%s]\n", __func__, type); */
      sp_str_free(&tmp_str);
    } else {
      tmp = find_direct_chld_by_type(subject, "type_identifier");
      if (!ts_node_is_null(tmp)) {
        /* $type_identifier $field_identifier;
         * Example:
         *  type_t type0;
         *  gint int0;
         */
        type = sp_struct_value(ctx, tmp);
        /* fprintf(stderr, "%s:3 [%s]\n", __func__, type); */
      } else {
        tmp = find_direct_chld_by_type(subject, "enum_specifier");
        if (!ts_node_is_null(tmp)) {
          TSNode type_id;

          type_id = find_direct_chld_by_type(tmp, "type_identifier");
          if (!ts_node_is_null(type_id)) {
            type = sp_struct_value(ctx, type_id);
            /* fprintf(stderr, "%s:4 [%s]\n", __func__, type); */
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
xx(struct sp_ts_Context *ctx,
   struct arg_list *result,
   TSNode subject,
   const char *identifier);

static struct arg_list *
__field_name(struct sp_ts_Context *ctx, TSNode subject, const char *identifier)
{
  struct arg_list *result = NULL;
  result                  = calloc(1, sizeof(*result));

  fprintf(stderr, "%s:{\n", __func__);
  debug_subtypes_rec(ctx, subject, 0);

  TSNode init_decl = find_direct_chld_by_type(subject, "init_declarator");
  if (!ts_node_is_null(init_decl)) {
    TSNode ptr_decl;
    ptr_decl = find_direct_chld_by_type(init_decl, "pointer_declarator");
    if (!ts_node_is_null(ptr_decl)) {
      __rec_search(ctx, ptr_decl, identifier, 1, &result->pointer);
    }
    xx(ctx, result, init_decl, identifier);
  } else {
    xx(ctx, result, subject, identifier);
  }

  /* fprintf(stderr, "%s:}\n", __func__); */
  return result;
}

static struct arg_list *
xx(struct sp_ts_Context *ctx,
   struct arg_list *result,
   TSNode subject,
   const char *identifier)
{
  fprintf(stderr, "  %s:{\n", __func__);
  debug_subtypes_rec(ctx, subject, 1);
  TSNode ptr_decl = find_direct_chld_by_type(subject, "pointer_declarator");
  if (!ts_node_is_null(ptr_decl)) {
    TSNode id_decl = find_rec_chld_by_type(subject, identifier);
    if (!ts_node_is_null(id_decl)) {
      struct arg_list *rit = result;
      if (rit->variable) {
        rit->next = calloc(1, sizeof(*rit));
        rit       = rit->next;
      }
      rit->variable = sp_struct_value(ctx, id_decl);
      __rec_search(ctx, ptr_decl, identifier, 1, &result->pointer);
    }
  } else {
    TSNode id_decl = find_direct_chld_by_type(subject, identifier);
    if (!ts_node_is_null(id_decl)) {
      result->variable = sp_struct_value(ctx, id_decl);
    } else {
      TSNode tmp;
      tmp = find_direct_chld_by_type(subject, "array_declarator");
      if (!ts_node_is_null(tmp)) {
        uint32_t i;
        TSNode field_id;
        bool start_found = false;

        for (i = 0; i < ts_node_child_count(tmp); ++i) {
          TSNode child = ts_node_child(tmp, i);
          if (start_found) {
            if (strcmp(ts_node_type(child), "]") == 0) {
            } else {
              free(result->variable_array_length);
              result->variable_array_length = sp_struct_value(ctx, child);
            }
            start_found = false;
          } else if (strcmp(ts_node_type(child), "[") == 0) {
            start_found = true;
          }
        } //for

        field_id = find_direct_chld_by_type(tmp, identifier);
        if (!ts_node_is_null(field_id)) {
          if (result->variable_array_length) {
            /* char a[LENGTH] = ""; */
            result->is_array = true;
          } else {
            /* char a[] = ""; // auto length */
            ++result->pointer;
          }
          result->variable = sp_struct_value(ctx, field_id);
        }
      } else {
        TSNode fun_decl;
        fun_decl = find_direct_chld_by_type(subject, "function_declarator");

        if (!ts_node_is_null(fun_decl)) {
          TSNode par_decl;
          par_decl =
            find_direct_chld_by_type(fun_decl, "parenthesized_declarator");
          fprintf(stderr, "%s:4\n", __func__);

          /* fprintf(stderr, "%s: 1\n", __func__); */
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
  /* fprintf(stderr, "  %s:}\n", __func__); */
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

static bool
__format_libxml2(struct sp_ts_Context *ctx,
                 struct arg_list *result,
                 const char *pprefix)
{
  (void)ctx;
  if (strcmp(result->type, "xmlNode") == 0 ||
      strcmp(result->type, "xmlNodePtr") == 0) {
    sp_str buf_tmp;
    sp_str_init(&buf_tmp, 0);

    result->format = "name[%s]line[%u]";
    if (result->pointer || strcmp(result->type, "xmlNodePtr") == 0) {
      sp_str_appends(&buf_tmp, pprefix, result->variable, " ? ", //
                     pprefix, result->variable, "->name", " : \"(NULL)\", ",
                     NULL);
      sp_str_appends(&buf_tmp, pprefix, result->variable, " ? ", //
                     pprefix, result->variable, "->line", "1337", NULL);
    } else {
      sp_str_appends(&buf_tmp, pprefix, result->variable, ".name, ", NULL);
      sp_str_appends(&buf_tmp, pprefix, result->variable, ".line", NULL);
    }
    free(result->complex_raw);
    result->complex_raw    = strdup(sp_str_c_str(&buf_tmp));
    result->complex_printf = true;
    sp_str_free(&buf_tmp);
    return true;
  }

  if (strcmp(result->type, "xmlNotation") == 0 ||
      strcmp(result->type, "xmlNotationPtr") == 0) {
    sp_str buf_tmp;
    sp_str_init(&buf_tmp, 0);

    result->format = "name[%s]line[%u]";
    if (result->pointer || strcmp(result->type, "xmlNodePtr") == 0) {
      sp_str_appends(&buf_tmp, pprefix, result->variable, " ? ", //
                     pprefix, result->variable, "->name", " : \"(NULL)\", ",
                     NULL);
      sp_str_appends(&buf_tmp, pprefix, result->variable, " ? ", //
                     pprefix, result->variable, "->line", "1337", NULL);
    } else {
      sp_str_appends(&buf_tmp, pprefix, result->variable, ".name, ", NULL);
      sp_str_appends(&buf_tmp, pprefix, result->variable, ".line", NULL);
    }
    free(result->complex_raw);
    result->complex_raw    = strdup(sp_str_c_str(&buf_tmp));
    result->complex_printf = true;
    sp_str_free(&buf_tmp);
    return true;
  }

  if (strcmp(result->type, "xmlAtrr") == 0 ||
      strcmp(result->type, "xmlAttrPtr") == 0) {
    sp_str buf_tmp;
    sp_str_init(&buf_tmp, 0);

    result->format = "name[%s]";
    if (result->pointer || strcmp(result->type, "xmlAttrPtr") == 0) {
      sp_str_appends(&buf_tmp, pprefix, result->variable, " ? ", //
                     pprefix, result->variable, "->name", " : \"(NULL)\"",
                     NULL);
    } else {
      sp_str_appends(&buf_tmp, pprefix, result->variable, ".name", NULL);
    }
    free(result->complex_raw);
    result->complex_raw    = strdup(sp_str_c_str(&buf_tmp));
    result->complex_printf = true;
    sp_str_free(&buf_tmp);
    return true;
  }

  if (strcmp(result->type, "xmlAttribute") == 0) {
    sp_str buf_tmp;
    sp_str_init(&buf_tmp, 0);

    result->format = "name[%s]";
    if (result->pointer) {
      sp_str_appends(&buf_tmp, pprefix, result->variable, " ? ", //
                     pprefix, result->variable, "->name", " : \"(NULL)\"",
                     NULL);
    } else {
      sp_str_appends(&buf_tmp, pprefix, result->variable, ".name", NULL);
    }
    free(result->complex_raw);
    result->complex_raw    = strdup(sp_str_c_str(&buf_tmp));
    result->complex_printf = true;
    sp_str_free(&buf_tmp);
    return true;
  }

  if (strcmp(result->type, "xmlDoc") == 0) {
    sp_str buf_tmp;
    sp_str_init(&buf_tmp, 0);

    result->format = "name[%s]";
    if (result->pointer) {
      sp_str_appends(&buf_tmp, pprefix, result->variable, " ? ", //
                     pprefix, result->variable, "->name", " : \"(NULL)\"",
                     NULL);
    } else {
      sp_str_appends(&buf_tmp, pprefix, result->variable, ".name", NULL);
    }
    free(result->complex_raw);
    result->complex_raw    = strdup(sp_str_c_str(&buf_tmp));
    result->complex_printf = true;
    sp_str_free(&buf_tmp);
    return true;
  }

  if (strcmp(result->type, "xmlElement") == 0 ||
      strcmp(result->type, "xmlElementPtr") == 0) {
    sp_str buf_tmp;
    sp_str_init(&buf_tmp, 0);

    result->format = "name[%s]";
    if (result->pointer || strcmp(result->type, "xmlElementPtr") == 0) {
      sp_str_appends(&buf_tmp, pprefix, result->variable, " ? ", //
                     pprefix, result->variable, "->name", " : \"(NULL)\"",
                     NULL);
    } else {
      sp_str_appends(&buf_tmp, pprefix, result->variable, ".name", NULL);
    }
    free(result->complex_raw);
    result->complex_raw    = strdup(sp_str_c_str(&buf_tmp));
    result->complex_printf = true;
    sp_str_free(&buf_tmp);
    return true;
  }

  if (strcmp(result->type, "xmlEntity") == 0 ||
      strcmp(result->type, "xmlEntityPtr") == 0) {
    sp_str buf_tmp;
    sp_str_init(&buf_tmp, 0);

    result->format = "name[%s]URI[%s]";
    if (result->pointer || strcmp(result->type, "xmlEntityPtr") == 0) {
      sp_str_appends(&buf_tmp, pprefix, result->variable, " ? ", //
                     pprefix, result->variable, "->name", " : \"(NULL)\", ",
                     NULL);
      sp_str_appends(&buf_tmp, pprefix, result->variable, " ? ", //
                     pprefix, result->variable, "->URI", " : \"(NULL)\", ",
                     NULL);
    } else {
      sp_str_appends(&buf_tmp, pprefix, result->variable, ".name, ", NULL);
      sp_str_appends(&buf_tmp, pprefix, result->variable, ".URI, ", NULL);
    }
    free(result->complex_raw);
    result->complex_raw    = strdup(sp_str_c_str(&buf_tmp));
    result->complex_printf = true;
    sp_str_free(&buf_tmp);
    return true;
  }

  if (strcmp(result->type, "xmlDoc") == 0 ||
      strcmp(result->type, "xmlDocPtr") == 0) {
    sp_str buf_tmp;
    sp_str_init(&buf_tmp, 0);

    result->format = "URL[%s]";
    if (result->pointer || strcmp(result->type, "xmlDocPtr") == 0) {
      sp_str_appends(&buf_tmp, pprefix, result->variable, " ? ", //
                     pprefix, result->variable, "->URL", " : \"(NULL)\"", NULL);
    } else {
      sp_str_appends(&buf_tmp, pprefix, result->variable, ".URL", NULL);
    }
    free(result->complex_raw);
    result->complex_raw    = strdup(sp_str_c_str(&buf_tmp));
    result->complex_printf = true;
    sp_str_free(&buf_tmp);
    return true;
  }

  if (strcmp(result->type, "xmlID") == 0 ||
      strcmp(result->type, "xmlIDPtr") == 0) {
    sp_str buf_tmp;
    sp_str_init(&buf_tmp, 0);

    result->format = "name[%s]line[%u]";
    if (result->pointer || strcmp(result->type, "xmlIDPtr") == 0) {
      sp_str_appends(&buf_tmp, pprefix, result->variable, " ? ", //
                     pprefix, result->variable, "->name", " : \"(NULL)\", ",
                     NULL);
      sp_str_appends(&buf_tmp, pprefix, result->variable, " ? ", //
                     pprefix, result->variable, "->lineno", "1337", NULL);
    } else {
      sp_str_appends(&buf_tmp, pprefix, result->variable, ".name, ", NULL);
      sp_str_appends(&buf_tmp, pprefix, result->variable, ".lineno", NULL);
    }
    free(result->complex_raw);
    result->complex_raw    = strdup(sp_str_c_str(&buf_tmp));
    result->complex_printf = true;
    sp_str_free(&buf_tmp);
    return true;
  }

  if (strcmp(result->type, "xmlRef") == 0 ||
      strcmp(result->type, "xmlRefPtr") == 0) {
    sp_str buf_tmp;
    sp_str_init(&buf_tmp, 0);

    result->format = "name[%s]line[%u]";
    if (result->pointer || strcmp(result->type, "xmlRefPtr") == 0) {
      sp_str_appends(&buf_tmp, pprefix, result->variable, " ? ", //
                     pprefix, result->variable, "->name", " : \"(NULL)\", ",
                     NULL);
      sp_str_appends(&buf_tmp, pprefix, result->variable, " ? ", //
                     pprefix, result->variable, "->lineno", "1337", NULL);
    } else {
      sp_str_appends(&buf_tmp, pprefix, result->variable, ".name, ", NULL);
      sp_str_appends(&buf_tmp, pprefix, result->variable, ".lineno", NULL);
    }
    free(result->complex_raw);
    result->complex_raw    = strdup(sp_str_c_str(&buf_tmp));
    result->complex_printf = true;
    sp_str_free(&buf_tmp);
    return true;
  }

  return false;
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
      if (strcmp(result->type, "GError") == 0) {
      } else {
        sp_str buf_tmp;
        sp_str_init(&buf_tmp, 0);
        result->format = "%p";
        sp_str_appends(&buf_tmp, "(void*)", pprefix, result->variable, NULL);
        free(result->complex_raw);
        result->complex_raw    = strdup(sp_str_c_str(&buf_tmp));
        result->complex_printf = true;

        sp_str_free(&buf_tmp);
      }
    } else if (strcmp(result->type, "gboolean") == 0 || //
               strcmp(result->type, "bool") == 0 || //
               strcmp(result->type, "boolean") == 0) {
      sp_str buf_tmp;
      sp_str_init(&buf_tmp, 0);
      result->format = "%s";

      if (result->pointer) {
        sp_str_appends(&buf_tmp, "!", pprefix, result->variable,
                       " ? \"(NULL)\" : *", pprefix, result->variable, NULL);
      } else {
        sp_str_appends(&buf_tmp, pprefix, result->variable, NULL);
      }
      sp_str_appends(&buf_tmp, " ? \"TRUE\" : \"FALSE\"", NULL);
      free(result->complex_raw);
      result->complex_raw    = strdup(sp_str_c_str(&buf_tmp));
      result->complex_printf = true;

      sp_str_free(&buf_tmp);
    } else if (strcmp(result->type, "mode_t") == 0) {
      if (result->pointer) {
      } else {
        sp_str buf_tmp;
        sp_str_init(&buf_tmp, 0);
        result->format = "%c%c%c%c%c%c%c%c%c";

        sp_str_appends(&buf_tmp, pprefix, result->variable,
                       " & S_IRUSR ? 'r' : '-', ", NULL);
        sp_str_appends(&buf_tmp, pprefix, result->variable,
                       " & S_IWUSR ? 'w' : '-', ", NULL);
        sp_str_appends(&buf_tmp, pprefix, result->variable,
                       " & S_IXUSR ? 'x' : '-', ", NULL);

        sp_str_appends(&buf_tmp, pprefix, result->variable,
                       " & S_IRGRP ? 'r' : '-', ", NULL);
        sp_str_appends(&buf_tmp, pprefix, result->variable,
                       " & S_IWGRP ? 'w' : '-', ", NULL);
        sp_str_appends(&buf_tmp, pprefix, result->variable,
                       " & S_IXGRP ? 'x' : '-', ", NULL);

        sp_str_appends(&buf_tmp, pprefix, result->variable,
                       " & S_IROTH ? 'r' : '-', ", NULL);
        sp_str_appends(&buf_tmp, pprefix, result->variable,
                       " & S_IWOTH ? 'w' : '-', ", NULL);
        sp_str_appends(&buf_tmp, pprefix, result->variable,
                       " & S_IXOTH ? 'x' : '-'", NULL);

        free(result->complex_raw);
        result->complex_raw    = strdup(sp_str_c_str(&buf_tmp));
        result->complex_printf = true;

        sp_str_free(&buf_tmp);
      }
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
               strcmp(result->type, "int8_t") == 0 ||
               strcmp(result->type, "xmlChar") == 0) {
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
               strcmp(result->type, "GMutex") == 0 ||
               strcmp(result->type, "GMutexLocker") == 0 ||
               strcmp(result->type, "GThreadPool") == 0 ||
               strcmp(result->type, "GRecMutex") == 0 ||
               strcmp(result->type, "GRWLock") == 0 ||
               strcmp(result->type, "GCond") == 0 ||
               strcmp(result->type, "GOnce") == 0 ||
               strcmp(result->type, "struct mutex") == 0) {
      /* fprintf(stderr, "type[%s]\n", type); */
    } else if (strcmp(result->type, "sd_bus") == 0) {
      sp_str buf_tmp;
      sp_str_init(&buf_tmp, 0);

#if 0
      result->format = "%p:open[%s]ready[%s]";
      if (result->pointer) {
        sp_str_appends(&buf_tmp, "(void *)", pprefix, result->variable, NULL);
      } else {
        sp_str_appends(&buf_tmp, "(void *)&", pprefix, result->variable, NULL);
      }
      sp_str_append(&buf_tmp, ", ");

      if (result->pointer) {
        sp_str_appends(&buf_tmp, pprefix, result->variable, NULL);
        sp_str_append(&buf_tmp, " ? sd_bus_is_open(");
        sp_str_appends(&buf_tmp, pprefix, result->variable, NULL);
        sp_str_append(&buf_tmp, ") ? \"TRUE\" : \"FALSE\" : \"NULL\"");
      } else {
        sp_str_append(&buf_tmp, "sd_bus_is_open(&");
        sp_str_appends(&buf_tmp, pprefix, result->variable, NULL);
        sp_str_append(&buf_tmp, ") ? \"TRUE\" : \"FALSE\"");
      }
      sp_str_append(&buf_tmp, ", ");

      if (result->pointer) {
        sp_str_appends(&buf_tmp, pprefix, result->variable, NULL);
        sp_str_append(&buf_tmp, " ? sd_bus_is_ready(");
        sp_str_appends(&buf_tmp, pprefix, result->variable, NULL);
        sp_str_append(&buf_tmp, ") ? \"TRUE\" : \"FALSE\" : \"NULL\"");
      } else {
        sp_str_append(&buf_tmp, "sd_bus_is_ready(&");
        sp_str_appends(&buf_tmp, pprefix, result->variable, NULL);
        sp_str_append(&buf_tmp, ") ? \"TRUE\" : \"FALSE\"");
      }
#else
      result->format = "%p:open[%d]ready[%d]";
      if (result->pointer) {
        sp_str_appends(&buf_tmp, "(void *)", pprefix, result->variable, NULL);
      } else {
        sp_str_appends(&buf_tmp, "(void *)&", pprefix, result->variable, NULL);
      }
      sp_str_append(&buf_tmp, ", ");

      if (result->pointer) {
        sp_str_appends(&buf_tmp, pprefix, result->variable, NULL);
        sp_str_append(&buf_tmp, " ? sd_bus_is_open(");
        sp_str_appends(&buf_tmp, pprefix, result->variable, NULL);
        sp_str_append(&buf_tmp, ") : 1337");
      } else {
        sp_str_append(&buf_tmp, "sd_bus_is_open(&");
        sp_str_appends(&buf_tmp, pprefix, result->variable, NULL);
        sp_str_append(&buf_tmp, ")");
      }
      sp_str_append(&buf_tmp, ", ");

      if (result->pointer) {
        sp_str_appends(&buf_tmp, pprefix, result->variable, NULL);
        sp_str_append(&buf_tmp, " ? sd_bus_is_ready(");
        sp_str_appends(&buf_tmp, pprefix, result->variable, NULL);
        sp_str_append(&buf_tmp, ") : 1337");
      } else {
        sp_str_append(&buf_tmp, "sd_bus_is_ready(&");
        sp_str_appends(&buf_tmp, pprefix, result->variable, NULL);
        sp_str_append(&buf_tmp, ")");
      }

#endif
      free(result->complex_raw);
      result->complex_raw    = strdup(sp_str_c_str(&buf_tmp));
      result->complex_printf = true;

      sp_str_free(&buf_tmp);

    } else if (strcmp(result->type, "GObject") == 0) {
#if 0
typedef struct _GObject                  GObject;
struct  _GObject {
  GTypeInstance  g_type_instance;
  /*< private >*/
  volatile guint ref_count;
  GData         *qdata;
};

TODO typedef struct _GTypeQuery		GTypeQuery;
struct _GTypeQuery {
  GType		type;
  const gchar  *type_name;
  guint		class_size;
  guint		instance_size;
};
#endif
      sp_str buf_tmp;
      sp_str_init(&buf_tmp, 0);

      result->format = "%s";
      if (result->pointer) {
        sp_str_appends(&buf_tmp, pprefix, result->variable, " && ", //
                       pprefix, result->variable, "->g_type_instance.g_class",
                       " ? g_type_name(", pprefix, result->variable,
                       "->g_type_instance.g_class->g_type)", //
                       " : \"(NULL)\"", NULL);
      } else {
        sp_str_appends(&buf_tmp, pprefix, result->variable,
                       ".g_type_instance.g_class", //
                       " ? g_type_name(", pprefix, result->variable,
                       ".g_type_instance.g_class->g_type)", " : \"(NULL)\"",
                       NULL);
      }
      free(result->complex_raw);
      result->complex_raw    = strdup(sp_str_c_str(&buf_tmp));
      result->complex_printf = true;
      sp_str_free(&buf_tmp);
    } else if (strcmp(result->type, "GObjectClass") == 0) {
#if 0
struct  _GObjectClass {
  GTypeClass   g_type_class;
  /*< private >*/
  GSList      *construct_properties;
  /*< public >*/
  /* seldom overridden */
  GObject*   (*constructor)     (GType                  type, guint                  n_construct_properties, GObjectConstructParam *construct_properties);
  /* overridable methods */
  void       (*set_property)		(GObject        *object, guint           property_id, const GValue   *value, GParamSpec     *pspec);
  void       (*get_property)		(GObject        *object, guint           property_id, GValue         *value, GParamSpec     *pspec);
  void       (*dispose)			(GObject        *object);
  void       (*finalize)		(GObject        *object);
  /* seldom overridden */
  void       (*dispatch_properties_changed) (GObject      *object, guint	   n_pspecs, GParamSpec  **pspecs);
  /* signals */
  void	     (*notify)			(GObject	*object, GParamSpec	*pspec);
  /* called when done constructing */
  void	     (*constructed)		(GObject	*object);
  /*< private >*/
  gsize		flags;
};
#endif
    } else if (strcmp(result->type, "GTypeInstance") == 0 ||
               strcmp(result->type, "_GTypeInstance") == 0) {
#if 0
typedef struct _GTypeInstance           GTypeInstance;
struct _GTypeInstance {
  /*< private >*/
  GTypeClass *g_class;
};
#endif
      sp_str buf_tmp;
      sp_str_init(&buf_tmp, 0);

      result->format = "%s";
      if (result->pointer) {
        sp_str_appends(&buf_tmp, pprefix, result->variable, " && ", //
                       pprefix, result->variable, "->g_class", //
                       " ? g_type_name(", pprefix, result->variable,
                       "->g_class->g_type)", //
                       " : \"(NULL)\"", NULL);
      } else {
        sp_str_appends(&buf_tmp, pprefix, result->variable, "->g_class",
                       " ? g_type_name(", pprefix, result->variable,
                       ".g_class->g_type)", //
                       " : \"(NULL)\"", NULL);
      }
      free(result->complex_raw);
      result->complex_raw    = strdup(sp_str_c_str(&buf_tmp));
      result->complex_printf = true;
      sp_str_free(&buf_tmp);
    } else if (strcmp(result->type, "GTypeClass") == 0 ||
               strcmp(result->type, "_GTypeClass") == 0) {
#if 0
typedef struct _GTypeClass              GTypeClass;
struct _GTypeClass {
  /*< private >*/
  GType g_type;
};
#endif
      sp_str buf_tmp;
      sp_str_init(&buf_tmp, 0);

      result->format = "%s";
      if (result->pointer) {
        sp_str_appends(&buf_tmp, pprefix, result->variable, " ?", //
                       " g_type_name(", pprefix, result->variable, "->g_type)",
                       " : \"(NULL)\"", NULL);
      } else {
        sp_str_appends(&buf_tmp, "g_type_name(", pprefix, result->variable,
                       ".g_type)", NULL);
      }
      free(result->complex_raw);
      result->complex_raw    = strdup(sp_str_c_str(&buf_tmp));
      result->complex_printf = true;
      sp_str_free(&buf_tmp);
    } else if (strcmp(result->type, "GValue") == 0 ||
               strcmp(result->type, "_GValue") == 0) {
#if 0
struct _GValue {
  GType		g_type;
  union {
    gint	v_int;
    guint	v_uint;
    glong	v_long;
    gulong	v_ulong;
    gint64      v_int64;
    guint64     v_uint64;
    gfloat	v_float;
    gdouble	v_double;
    gpointer	v_pointer;
  } data[2];
};
#endif
      sp_str buf_tmp;
      sp_str_init(&buf_tmp, 0);

      result->format = "type:%s";
      if (result->pointer) {
        sp_str_appends(&buf_tmp, pprefix, result->variable, " ?", //
                       " g_type_name(", pprefix, result->variable, "->g_type)",
                       " : \"(NULL)\"", NULL);
      } else {
        sp_str_appends(&buf_tmp, "g_type_name(", pprefix, result->variable,
                       ".g_type)", NULL);
      }
      free(result->complex_raw);
      result->complex_raw    = strdup(sp_str_c_str(&buf_tmp));
      result->complex_printf = true;
      sp_str_free(&buf_tmp);
    } else if (strcmp(result->type, "GType") == 0) {
      sp_str buf_tmp;
      sp_str_init(&buf_tmp, 0);

      result->format = "%s";
      if (result->pointer) {
        sp_str_appends(&buf_tmp, pprefix, result->variable, " ?",
                       " g_type_name(*", pprefix, result->variable, ")",
                       " : \"(NULL)\"", NULL);
      } else {
        sp_str_appends(&buf_tmp, "g_type_name(", pprefix, result->variable, ")",
                       NULL);
      }
      free(result->complex_raw);
      result->complex_raw    = strdup(sp_str_c_str(&buf_tmp));
      result->complex_printf = true;
      sp_str_free(&buf_tmp);
    } else if (strcmp(result->type, "GError") == 0) {
      sp_str buf_tmp;
      sp_str_init(&buf_tmp, 0);

      result->format = "%s";
      if (result->pointer) {
        sp_str_appends(&buf_tmp, pprefix, result->variable, " ? ", //
                       pprefix, result->variable, "->message", //
                       " : \"(NULL)\"", NULL);
      } else {
        sp_str_appends(&buf_tmp, pprefix, result->variable, ".message", NULL);
      }
      free(result->complex_raw);
      result->complex_raw    = strdup(sp_str_c_str(&buf_tmp));
      result->complex_printf = true;
      sp_str_free(&buf_tmp);
    } else if (strcmp(result->type, "XmlNode") == 0) {
      sp_str buf_tmp;
      sp_str_init(&buf_tmp, 0);

      result->format = "%s";
      if (result->pointer) {
        sp_str_appends(&buf_tmp, pprefix, result->variable, " ? ", //
                       pprefix, result->variable, "->name", //
                       " : \"(NULL)\"", NULL);
      } else {
        sp_str_appends(&buf_tmp, pprefix, result->variable, ".name", NULL);
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
                       ")", //
                       " : ", "-1337", NULL);
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
        sp_str_appends(&buf_tmp, pprefix, result->variable,
                       " ?", //
                       " \"SOME\" : \"(NULL)\"", NULL);
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
    } else if (strcmp(result->type, "gid_t") == 0 ||
               strcmp(result->type, "uid_t") == 0) {
      sp_str buf_tmp;
      sp_str_init(&buf_tmp, 0);
      result->format = "%u";
      if (result->pointer) {
        sp_str_appends(&buf_tmp, pprefix, result->variable, " ? ",
                       "(unsigned int)", pprefix, result->variable, " : 1337",
                       NULL);
      } else {
        sp_str_appends(&buf_tmp, "(unsigned int)", pprefix, result->variable,
                       NULL);
      }
      free(result->complex_raw);
      result->complex_raw    = strdup(sp_str_c_str(&buf_tmp));
      result->complex_printf = true;
      sp_str_free(&buf_tmp);
    } else if (strcmp(result->type, "pid_t") == 0) {
      sp_str buf_tmp;
      sp_str_init(&buf_tmp, 0);
      result->format = "%u";
      if (result->pointer) {
        sp_str_appends(&buf_tmp, pprefix, result->variable, " ? ",
                       "(unsigned int)", pprefix, result->variable, " : 1337",
                       NULL);
      } else {
        sp_str_appends(&buf_tmp, "(unsigned int)", pprefix, result->variable,
                       NULL);
      }
      free(result->complex_raw);
      result->complex_raw    = strdup(sp_str_c_str(&buf_tmp));
      result->complex_printf = true;
      sp_str_free(&buf_tmp);
    } else if (strcmp(result->type, "passwd") == 0) {
      sp_str buf_tmp;
      sp_str_init(&buf_tmp, 0);
      result->format = "name[%s]uid[%u]gid[%u]";
      if (result->pointer) {
        sp_str_appends(&buf_tmp, pprefix, result->variable, " ? ", pprefix,
                       result->variable, "->pw_name : \"\",", NULL);
        sp_str_appends(&buf_tmp, pprefix, result->variable, " ? (unsigned int)",
                       pprefix, result->variable, "->pw_uid : 1337,", NULL);
        sp_str_appends(&buf_tmp, pprefix, result->variable, " ? (unsigned int)",
                       pprefix, result->variable, "->pw_gid : 1337", NULL);
      } else {
        sp_str_appends(&buf_tmp, pprefix, result->variable, ".pw_name, ", NULL);
        sp_str_appends(&buf_tmp, "(unsigned int)", pprefix, result->variable,
                       ".pw_uid, ", NULL);
        sp_str_appends(&buf_tmp, "(unsigned int)", pprefix, result->variable,
                       ".pw_gid", NULL);
      }
      free(result->complex_raw);
      result->complex_raw    = strdup(sp_str_c_str(&buf_tmp));
      result->complex_printf = true;
      sp_str_free(&buf_tmp);

    } else if (strcmp(result->type, "group") == 0) {
      sp_str buf_tmp;
      sp_str_init(&buf_tmp, 0);
      result->format = "name[%s]gid[%u]";
      if (result->pointer) {
        sp_str_appends(&buf_tmp, pprefix, result->variable, " ? ", pprefix,
                       result->variable, "->gr_name : \"\", ", NULL);
        sp_str_appends(&buf_tmp, pprefix, result->variable, " ? (unsigned int)",
                       pprefix, result->variable, "->gr_gid : 1337", NULL);
      } else {
        sp_str_appends(&buf_tmp, pprefix, result->variable, ".gr_name, ", NULL);
        sp_str_appends(&buf_tmp, "(unsigned int)", pprefix, result->variable,
                       ".gr_gid", NULL);
      }
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
                       " : \"(NULL)\"", NULL);
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
                       ", FALSE)", " : \"(NULL)\"", NULL);
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
    } else if (strcmp(result->type, "GArray") == 0 ||
               strcmp(result->type, "GPtrArray") == 0) {
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

    } else if (strcmp(result->type, "GList") == 0) {
      result->format = "%p";
      sp_str buf_tmp;
      sp_str_init(&buf_tmp, 0);
      sp_str_append(&buf_tmp, "(void*)");
      if (result->pointer) {
      } else {
        sp_str_append(&buf_tmp, "&");
      }
      sp_str_appends(&buf_tmp, pprefix, result->variable, NULL);
      free(result->complex_raw);
      result->complex_raw    = strdup(sp_str_c_str(&buf_tmp));
      result->complex_printf = true;
      sp_str_free(&buf_tmp);

    } else if (strcmp(result->type, "GDBusMethodInvocation") == 0) {
      /* https://www.freedesktop.org/software/gstreamer-sdk/data/docs/2012.5/gio/GDBusMethodInvocation.html#g-dbus-method-invocation-get-sender */
      result->format = "%s";
      sp_str buf_tmp;
      sp_str_init(&buf_tmp, 0);
      if (result->pointer) {
        sp_str_appends(&buf_tmp, pprefix, result->variable,
                       " ? g_dbus_method_invocation_get_sender(", pprefix,
                       result->variable, ")", " : \"(NULL)\"", NULL);
      } else {
        sp_str_appends(&buf_tmp, "g_dbus_method_invocation_get_sender(&",
                       pprefix, result->variable, ")", NULL);
      }
      free(result->complex_raw);
      result->complex_raw    = strdup(sp_str_c_str(&buf_tmp));
      result->complex_printf = true;
      sp_str_free(&buf_tmp);

    } else if (strcmp(result->type, "pollfd") == 0) {
      sp_str buf_tmp;
      sp_str_init(&buf_tmp, 0);
      result->format = "fd[%d]";
      if (result->pointer) {
        /* TODO https://man7.org/linux/man-pages/man2/poll.2.html */
        /* add enum of possible events */
        sp_str_appends(&buf_tmp, pprefix, result->variable, " ? ", pprefix,
                       result->variable, "->fd", " : -1337", NULL);
      } else {
        sp_str_appends(&buf_tmp, pprefix, result->variable, ".fd", NULL);
      }

      free(result->complex_raw);
      result->complex_raw    = strdup(sp_str_c_str(&buf_tmp));
      result->complex_printf = true;
      sp_str_free(&buf_tmp);
    } else if (strcmp(result->type, "snd_ctl_t") == 0) {
      sp_str buf_tmp;
      sp_str_init(&buf_tmp, 0);
      result->format = "%s";
      if (result->pointer) {
        sp_str_appends(&buf_tmp, pprefix, result->variable, " ? snd_ctl_name(",
                       pprefix, result->variable, ")", " : \"(NULL)\"", NULL);
      } else {
        sp_str_appends(&buf_tmp, "snd_ctl_name(&", pprefix, result->variable,
                       ")", NULL);
      }

      free(result->complex_raw);
      result->complex_raw    = strdup(sp_str_c_str(&buf_tmp));
      result->complex_printf = true;
      sp_str_free(&buf_tmp);
    } else if (strcmp(result->type, "snd_ctl_event_t") == 0) {
      sp_str buf_tmp;
      sp_str_init(&buf_tmp, 0);
      result->format = "%s:%u,%u";

      if (result->pointer) {
        sp_str_appends(&buf_tmp, pprefix, result->variable,
                       " ? snd_ctl_event_elem_get_name(", pprefix,
                       result->variable, ")", " : \"(NULL)\", ", NULL);
        sp_str_appends(&buf_tmp, pprefix, result->variable,
                       " ? snd_ctl_event_elem_get_device(", pprefix,
                       result->variable, ")", " : 1337, ", NULL);
        sp_str_appends(&buf_tmp, pprefix, result->variable,
                       " ? snd_ctl_event_elem_get_subdevice(", pprefix,
                       result->variable, ")", " : 1337", NULL);
      } else {
        sp_str_appends(&buf_tmp, "snd_ctl_event_elem_get_name(&", pprefix,
                       result->variable, "), ", NULL);
        sp_str_appends(&buf_tmp, "snd_ctl_event_elem_get_device(&", pprefix,
                       result->variable, "), ", NULL);
        sp_str_appends(&buf_tmp, "snd_ctl_event_elem_get_subdevice(&", pprefix,
                       result->variable, ")", NULL);
      }

      free(result->complex_raw);
      result->complex_raw    = strdup(sp_str_c_str(&buf_tmp));
      result->complex_printf = true;
      sp_str_free(&buf_tmp);

    } else if (strcmp(result->type, "snd_ctl_card_info_t") == 0) {
      sp_str buf_tmp;
      sp_str_init(&buf_tmp, 0);
      result->format = "%s";
      if (result->pointer) {
        sp_str_appends(&buf_tmp, pprefix, result->variable,
                       " ? snd_ctl_card_info_get_name(", pprefix,
                       result->variable, ")", " : \"(NULL)\"", NULL);
      } else {
        sp_str_appends(&buf_tmp, "snd_ctl_card_info_get_name(&", pprefix,
                       result->variable, ")", NULL);
      }

      free(result->complex_raw);
      result->complex_raw    = strdup(sp_str_c_str(&buf_tmp));
      result->complex_printf = true;
      sp_str_free(&buf_tmp);

    } else if (strcmp(result->type, "snd_ctl_elem_type_t") == 0) {
      sp_str buf_tmp;
      sp_str_init(&buf_tmp, 0);
      result->format = "%s";
      if (result->pointer) {
        sp_str_appends(&buf_tmp, pprefix, result->variable,
                       " ? snd_ctl_elem_type_name(*", pprefix, result->variable,
                       ")", " : \"(NULL)\"", NULL);
      } else {
        sp_str_appends(&buf_tmp, "snd_ctl_elem_type_name(", pprefix,
                       result->variable, ")", NULL);
      }

      free(result->complex_raw);
      result->complex_raw    = strdup(sp_str_c_str(&buf_tmp));
      result->complex_printf = true;
      sp_str_free(&buf_tmp);
    } else if (strcmp(result->type, "snd_ctl_elem_value_t") == 0) {
      sp_str buf_tmp;
      sp_str_init(&buf_tmp, 0);
      result->format = "%s";
      if (result->pointer) {
        sp_str_appends(&buf_tmp, pprefix, result->variable,
                       " ? snd_ctl_elem_value_get_name(", pprefix,
                       result->variable, ")", " : \"(NULL)\"", NULL);
      } else {
        sp_str_appends(&buf_tmp, "snd_ctl_elem_value_get_name(&", pprefix,
                       result->variable, ")", NULL);
      }

      free(result->complex_raw);
      result->complex_raw    = strdup(sp_str_c_str(&buf_tmp));
      result->complex_printf = true;
      sp_str_free(&buf_tmp);

    } else if (strcmp(result->type, "snd_ctl_event_type_t") == 0) {
      sp_str buf_tmp;
      sp_str_init(&buf_tmp, 0);
      result->format = "%s";
      if (result->pointer) {
        sp_str_appends(&buf_tmp, pprefix, result->variable,
                       " ? snd_ctl_event_type_name(", pprefix, result->variable,
                       ")", " : \"(NULL)\"", NULL);
      } else {
        sp_str_appends(&buf_tmp, "snd_ctl_event_type_name(&", pprefix,
                       result->variable, ")", NULL);
      }

      free(result->complex_raw);
      result->complex_raw    = strdup(sp_str_c_str(&buf_tmp));
      result->complex_printf = true;
      sp_str_free(&buf_tmp);

    } else if (strcmp(result->type, "snd_ctl_t") == 0) {
      sp_str buf_tmp;
      sp_str_init(&buf_tmp, 0);
      result->format = "%s";
      if (result->pointer) {
        sp_str_appends(&buf_tmp, pprefix, result->variable, " ? snd_ctl_name(",
                       pprefix, result->variable, ")", " : \"(NULL)\"", NULL);
      } else {
        sp_str_appends(&buf_tmp, "snd_ctl_name(&", pprefix, result->variable,
                       ")", NULL);
      }

      free(result->complex_raw);
      result->complex_raw    = strdup(sp_str_c_str(&buf_tmp));
      result->complex_printf = true;
      sp_str_free(&buf_tmp);

    } else if (strcmp(result->type, "snd_ctl_elem_id_t") == 0) {
      sp_str buf_tmp;
      sp_str_init(&buf_tmp, 0);
      result->format = "%s";
      if (result->pointer) {
        sp_str_appends(&buf_tmp, pprefix, result->variable,
                       " ? snd_ctl_elem_id_get_name(", pprefix,
                       result->variable, ")", " : \"(NULL)\"", NULL);
      } else {
        sp_str_appends(&buf_tmp, "snd_ctl_elem_id_get_name(&", pprefix,
                       result->variable, ")", NULL);
      }

      free(result->complex_raw);
      result->complex_raw    = strdup(sp_str_c_str(&buf_tmp));
      result->complex_printf = true;
      sp_str_free(&buf_tmp);

    } else if (strcmp(result->type, "GHashTable") == 0) {
      sp_str buf_tmp;
      sp_str_init(&buf_tmp, 0);

      if (result->pointer) {
        result->format = "len[%u]";
        sp_str_appends(&buf_tmp, pprefix, result->variable,
                       " ? g_hash_table_size(", pprefix, result->variable, ")",
                       " : 1337", NULL);
      } else {
        result->format = "len[%u]";
        sp_str_appends(&buf_tmp, "g_hash_table_size(&", pprefix,
                       result->variable, ")", NULL);
      }
      free(result->complex_raw);
      result->complex_raw    = strdup(sp_str_c_str(&buf_tmp));
      result->complex_printf = true;
      sp_str_free(&buf_tmp);
    } else if (strcmp(result->type, "GDBusConnection") == 0) {
      sp_str buf_tmp;
      sp_str_init(&buf_tmp, 0);
      result->format = "%s";
      /* ALTERNATILVY g_dbus_connection_get_guid() instead */
      if (result->pointer) {
        sp_str_appends(&buf_tmp, pprefix, result->variable,
                       " ? g_dbus_connection_get_unique_name(", pprefix,
                       result->variable, ")", " : \"(NULL)\"", NULL);
      } else {
        sp_str_appends(&buf_tmp, "g_dbus_connection_get_unique_name(&", pprefix,
                       result->variable, ")", NULL);
      }
      free(result->complex_raw);
      result->complex_raw    = strdup(sp_str_c_str(&buf_tmp));
      result->complex_printf = true;
      sp_str_free(&buf_tmp);
    } else if (strcmp(result->type, "GPrivate") == 0) {
      sp_str buf_tmp;
      sp_str_init(&buf_tmp, 0);
      if (result->pointer) {
        result->format = "%p%s";
        sp_str_appends(&buf_tmp, pprefix, result->variable,
                       " ? (void*)g_private_get(", pprefix, result->variable,
                       ")", " : \"(NULL)\",", NULL);
        sp_str_appends(&buf_tmp, pprefix, result->variable,
                       " ? \"\" : \"(NULL)\"", NULL);
      } else {
        result->format = "%p";
        sp_str_appends(&buf_tmp, "(void*)g_private_get(&", pprefix,
                       result->variable, ")", NULL);
      }
      free(result->complex_raw);
      result->complex_raw    = strdup(sp_str_c_str(&buf_tmp));
      result->complex_printf = true;
      sp_str_free(&buf_tmp);

    } else if (strcmp(result->type, "FILE") == 0) {
      sp_str buf_tmp;
      sp_str_init(&buf_tmp, 0);
      result->format = "%p";
      if (result->pointer) {
        sp_str_appends(&buf_tmp, "(void *)", pprefix, result->variable, NULL);
      } else {
      }
      free(result->complex_raw);
      result->complex_raw    = strdup(sp_str_c_str(&buf_tmp));
      result->complex_printf = true;
      sp_str_free(&buf_tmp);

    } else if (strcmp(result->type, "GFile") == 0) {
      sp_str buf_tmp;
      sp_str_init(&buf_tmp, 0);
      result->format = "%s";
      if (result->pointer) {
        sp_str_appends(&buf_tmp, pprefix, result->variable,
                       " ? g_file_get_path(", pprefix, result->variable, ")",
                       " : \"(NULL)\"", NULL);
      } else {
        sp_str_appends(&buf_tmp, "g_file_get_path(&", pprefix, result->variable,
                       ")", NULL);
      }
      free(result->complex_raw);
      result->complex_raw    = strdup(sp_str_c_str(&buf_tmp));
      result->complex_printf = true;
      sp_str_free(&buf_tmp);
    } else if (strcmp(result->type, "GString") == 0) {
      sp_str buf_tmp;
      sp_str_init(&buf_tmp, 0);
      result->format = "%s";
      if (result->pointer) {
        sp_str_appends(&buf_tmp, pprefix, result->variable, " ? ", pprefix,
                       result->variable,
                       "->str"
                       " : \"(NULL)\"",
                       NULL);
      } else {
        sp_str_appends(&buf_tmp, pprefix, result->variable, ".str", NULL);
      }
      free(result->complex_raw);
      result->complex_raw    = strdup(sp_str_c_str(&buf_tmp));
      result->complex_printf = true;
      sp_str_free(&buf_tmp);
    } else if (strcmp(result->type, "GDBusProxy") == 0) {
      sp_str buf_tmp;
      sp_str_init(&buf_tmp, 0);
      result->format = "%s:%s";
      if (result->pointer) {
        sp_str_appends(&buf_tmp, pprefix, result->variable,
                       " ? g_dbus_proxy_get_object_path(", pprefix,
                       result->variable, ")", " : \"(NULL)\", ", NULL);
        sp_str_appends(&buf_tmp, pprefix, result->variable,
                       " ? g_dbus_proxy_get_interface_name(", pprefix,
                       result->variable, ")", " : \"(NULL)\"", NULL);
      } else {
        sp_str_appends(&buf_tmp, "g_dbus_proxy_get_object_path(&", pprefix,
                       result->variable, "), ", NULL);
        sp_str_appends(&buf_tmp, "g_dbus_proxy_get_interface_name(&", pprefix,
                       result->variable, ")", NULL);
      }
      free(result->complex_raw);
      result->complex_raw    = strdup(sp_str_c_str(&buf_tmp));
      result->complex_printf = true;
      sp_str_free(&buf_tmp);
    } else if (strcmp(result->type, "GDir") == 0) {
      sp_str buf_tmp;
      sp_str_init(&buf_tmp, 0);
      result->format = "%s";
      if (result->pointer) {
        sp_str_appends(&buf_tmp, pprefix, result->variable,
                       " ? g_dir_read_name(", pprefix, result->variable, ")",
                       " : \"(NULL)\"", NULL);
      } else {
        sp_str_appends(&buf_tmp, "g_dir_read_name(&", pprefix, result->variable,
                       ")", NULL);
      }
      free(result->complex_raw);
      result->complex_raw    = strdup(sp_str_c_str(&buf_tmp));
      result->complex_printf = true;
      sp_str_free(&buf_tmp);
    } else if (strcmp(result->type, "GParamSpec") == 0) {
      sp_str buf_tmp;
      sp_str_init(&buf_tmp, 0);
      result->format = "%s";
      if (result->pointer) {
        sp_str_appends(&buf_tmp, pprefix, result->variable, " ? ", pprefix,
                       result->variable, "->name", " : \"(NULL)\"", NULL);
      } else {
        sp_str_appends(&buf_tmp, pprefix, result->variable, ".name", NULL);
      }
      free(result->complex_raw);
      result->complex_raw    = strdup(sp_str_c_str(&buf_tmp));
      result->complex_printf = true;
      sp_str_free(&buf_tmp);

    } else if (strcmp(result->type, "sd_bus_message") == 0) {
      sp_str buf_tmp;
      sp_str_init(&buf_tmp, 0);
      result->format = "%s";
      if (result->pointer) {
        sp_str_appends(&buf_tmp, pprefix, result->variable,
                       " ? sd_bus_message_get_signature(", pprefix,
                       result->variable, ", true)", " : \"(NULL)\"", NULL);
      } else {
        sp_str_appends(&buf_tmp, "sd_bus_message_get_signature(&", pprefix,
                       result->variable, ")", NULL);
      }
      free(result->complex_raw);
      result->complex_raw    = strdup(sp_str_c_str(&buf_tmp));
      result->complex_printf = true;
      sp_str_free(&buf_tmp);

    } else if (strcmp(result->type, "sd_bus_error") == 0) {
      sp_str buf_tmp;
      sp_str_init(&buf_tmp, 0);
      result->format = "%s:%s";
      if (result->pointer) {
        sp_str_appends(&buf_tmp, pprefix, result->variable, " ? ", pprefix,
                       result->variable, "->name", " : \"(NULL)\", ", NULL);
        sp_str_appends(&buf_tmp, pprefix, result->variable, " ? ", pprefix,
                       result->variable, "->message", " : \"(NULL)\"", NULL);
      } else {
        sp_str_appends(&buf_tmp, pprefix, result->variable, ".name, ", NULL);
        sp_str_appends(&buf_tmp, pprefix, result->variable, ".message", NULL);
      }
      free(result->complex_raw);
      result->complex_raw    = strdup(sp_str_c_str(&buf_tmp));
      result->complex_printf = true;
      sp_str_free(&buf_tmp);
    } else if (strcmp(result->type, "snd_mixer_t") == 0) {
      result->format = "%p";
      sp_str buf_tmp;
      sp_str_init(&buf_tmp, 0);
      sp_str_append(&buf_tmp, "(void*)");
      if (result->pointer) {
      } else {
        sp_str_append(&buf_tmp, "&");
      }
      sp_str_appends(&buf_tmp, pprefix, result->variable, NULL);
      free(result->complex_raw);
      result->complex_raw    = strdup(sp_str_c_str(&buf_tmp));
      result->complex_printf = true;
      sp_str_free(&buf_tmp);

    } else if (strcmp(result->type, "snd_pcm") == 0) {
      //kernel
#if 0
struct snd_pcm {
	struct snd_card *card;
	struct list_head list;
	int device; /* device number */
	unsigned int info_flags;
	unsigned short dev_class;
	unsigned short dev_subclass;
	char id[64];
	char name[80];
	struct snd_pcm_str streams[2];
	struct mutex open_mutex;
	wait_queue_head_t open_wait;
	void *private_data;
	void (*private_free) (struct snd_pcm *pcm);
	bool internal; /* pcm is for internal use only */
	bool nonatomic; /* whole PCM operations are in non-atomic context */
};
#endif
      result->format = "%p:dev[%d]id[%s]name[%s]";
      sp_str buf_tmp;
      sp_str_init(&buf_tmp, 0);
      if (result->pointer) {
        sp_str_appends(&buf_tmp, "(void *)", pprefix, result->variable, ", ",
                       NULL);

        sp_str_appends(&buf_tmp, pprefix, result->variable, " ? ", NULL);
        sp_str_appends(&buf_tmp, pprefix, result->variable, "->device : 1337, ",
                       NULL);

        sp_str_appends(&buf_tmp, pprefix, result->variable, " ? ", NULL);
        sp_str_appends(&buf_tmp, pprefix, result->variable, "->name : \"\", ",
                       NULL);

        sp_str_appends(&buf_tmp, pprefix, result->variable, " ? ", NULL);
        sp_str_appends(&buf_tmp, pprefix, result->variable, "->id : \"\"",
                       NULL);
      } else {
        assert(false);
      }
      free(result->complex_raw);
      result->complex_raw    = strdup(sp_str_c_str(&buf_tmp));
      result->complex_printf = true;
      sp_str_free(&buf_tmp);

    } else if (strcmp(result->type, "snd_pcm_substream") == 0) {
#if 0
struct snd_pcm_substream {
	struct snd_pcm *pcm;
	struct snd_pcm_str *pstr;
	void *private_data;		/* copied from pcm->private_data */
	int number;
	char name[32];			/* substream name */
	int stream;			/* stream (direction) */
};
#endif
      result->format = "pcm[%p]number[%d]name[%s]stream[%d]";
      sp_str buf_tmp;
      sp_str_init(&buf_tmp, 0);
      if (result->pointer) {
        sp_str_appends(&buf_tmp, pprefix, result->variable, " ? ", NULL);
        sp_str_appends(&buf_tmp, pprefix, result->variable, "->pcm : NULL, ",
                       NULL);

        sp_str_appends(&buf_tmp, pprefix, result->variable, " ? ", NULL);
        sp_str_appends(&buf_tmp, pprefix, result->variable, "->number : 1337, ",
                       NULL);

        sp_str_appends(&buf_tmp, pprefix, result->variable, " ? ", NULL);
        sp_str_appends(&buf_tmp, pprefix, result->variable, "->name : \"\", ",
                       NULL);

        sp_str_appends(&buf_tmp, pprefix, result->variable, " ? ", NULL);
        sp_str_appends(&buf_tmp, pprefix, result->variable, "->stream : 1337",
                       NULL);
      } else {
        assert(false);
      }
      free(result->complex_raw);
      result->complex_raw    = strdup(sp_str_c_str(&buf_tmp));
      result->complex_printf = true;
      sp_str_free(&buf_tmp);

    } else if (strcmp(result->type, "snd_soc_dai") == 0) {
#if 0
struct snd_soc_dai {
	const char *name;
	int id;
	struct device *dev;
};
#endif
      result->format = "name[%s]id[%d]dev[%p]";
      sp_str buf_tmp;
      sp_str_init(&buf_tmp, 0);
      if (result->pointer) {
        sp_str_appends(&buf_tmp, pprefix, result->variable, " ? ", NULL);
        sp_str_appends(&buf_tmp, pprefix, result->variable, "->name : \"\", ",
                       NULL);

        sp_str_appends(&buf_tmp, pprefix, result->variable, " ? ", NULL);
        sp_str_appends(&buf_tmp, pprefix, result->variable, "->id : 1337, ",
                       NULL);

        sp_str_appends(&buf_tmp, pprefix, result->variable, " ? ", NULL);
        sp_str_appends(&buf_tmp, pprefix, result->variable, "->dev : NULL",
                       NULL);
      } else {
        assert(false);
      }
      free(result->complex_raw);
      result->complex_raw    = strdup(sp_str_c_str(&buf_tmp));
      result->complex_printf = true;
      sp_str_free(&buf_tmp);

    } else if (strcmp(result->type, "snd_pcm_runtime") == 0) {
#if 0
#if 0
struct snd_pcm_runtime {
	struct snd_pcm_substream *trigger_master;
	struct timespec trigger_tstamp;	/* trigger timestamp */
	bool trigger_tstamp_latched;     /* trigger timestamp latched in low-level driver/hardware */
	int overrange;
	snd_pcm_uframes_t avail_max;
	snd_pcm_uframes_t hw_ptr_base;	/* Position at buffer restart */
	snd_pcm_uframes_t hw_ptr_interrupt; /* Position at interrupt time */
	unsigned long hw_ptr_jiffies;	/* Time when hw_ptr is updated */
	unsigned long hw_ptr_buffer_jiffies; /* buffer time in jiffies */
	snd_pcm_sframes_t delay;	/* extra delay; typically FIFO size */
	u64 hw_ptr_wrap;                /* offset for hw_ptr due to boundary wrap-around */
	/* -- HW params -- */
	snd_pcm_access_t access;	/* access mode */
	snd_pcm_format_t format;	/* SNDRV_PCM_FORMAT_* */
	snd_pcm_subformat_t subformat;	/* subformat */
	unsigned int rate;		/* rate in Hz */
	unsigned int channels;		/* channels */
	snd_pcm_uframes_t period_size;	/* period size */
	unsigned int periods;		/* periods */
	snd_pcm_uframes_t buffer_size;	/* buffer size */
	snd_pcm_uframes_t min_align;	/* Min alignment for the format */
	size_t byte_align
};
#endif
      result->format = "name[%s]id[%d]dev[%p]";
      sp_str buf_tmp;
      sp_str_init(&buf_tmp, 0);
      if (result->pointer) {
        sp_str_appends(&buf_tmp, pprefix, result->variable, " ? ", NULL);
        sp_str_appends(&buf_tmp, pprefix, result->variable, "->name : \"\", ",
                       NULL);

        sp_str_appends(&buf_tmp, pprefix, result->variable, " ? ", NULL);
        sp_str_appends(&buf_tmp, pprefix, result->variable, "->id : 1337, ",
                       NULL);

        sp_str_appends(&buf_tmp, pprefix, result->variable, " ? ", NULL);
        sp_str_appends(&buf_tmp, pprefix, result->variable, "->dev : NULL",
                       NULL);
      } else {
        assert(false);
      }
      free(result->complex_raw);
      result->complex_raw    = strdup(sp_str_c_str(&buf_tmp));
      result->complex_printf = true;
      sp_str_free(&buf_tmp);
#endif
      /* TODO */
    } else if (strcmp(result->type, "snd_pcm_hardware") == 0) {
#if 0
struct snd_pcm_hardware {
  unsigned int info;		/* SNDRV_PCM_INFO_* */
  u64 formats;			/* SNDRV_PCM_FMTBIT_* */
  unsigned int rates;		/* SNDRV_PCM_RATE_* */
  unsigned int rate_min;		/* min rate */
  unsigned int rate_max;		/* max rate */
  unsigned int channels_min;	/* min channels */
  unsigned int channels_max;	/* max channels */
  size_t buffer_bytes_max;	/* max buffer size */
  size_t period_bytes_min;	/* min period size */
  size_t period_bytes_max;	/* max period size */
  unsigned int periods_min;	/* min # of periods */
  unsigned int periods_max;	/* max # of periods */
  size_t fifo_size;		/* fifo size in bytes */
};
#endif
      result->format =
        "info[%u]formats[%llu]rates[%u]rate_min[%u]rate_max[%u]channels_min[%u]"
        "channels_max[%u]buffer_bytes_max[%zu]period_bytes_min[%zu]period_"
        "bytes_max[%zu]periods_min[%u]periods_max[%u]fifo_size[%zu]}";
      sp_str buf_tmp;
      sp_str_init(&buf_tmp, 0);
      if (result->pointer) {
        sp_str_appends(&buf_tmp, pprefix, result->variable, " ? ", NULL);
        sp_str_appends(&buf_tmp, pprefix, result->variable, "->info : 1337, ",
                       NULL);

        sp_str_appends(&buf_tmp, pprefix, result->variable, " ? ", NULL);
        sp_str_appends(&buf_tmp, pprefix, result->variable,
                       "->formats : 1337, ", NULL);

        sp_str_appends(&buf_tmp, pprefix, result->variable, " ? ", NULL);
        sp_str_appends(&buf_tmp, pprefix, result->variable, "->rates : 1337, ",
                       NULL);

        sp_str_appends(&buf_tmp, pprefix, result->variable, " ? ", NULL);
        sp_str_appends(&buf_tmp, pprefix, result->variable,
                       "->rate_min : 1337, ", NULL);

        sp_str_appends(&buf_tmp, pprefix, result->variable, " ? ", NULL);
        sp_str_appends(&buf_tmp, pprefix, result->variable,
                       "->rate_max : 1337, ", NULL);

        sp_str_appends(&buf_tmp, pprefix, result->variable, " ? ", NULL);
        sp_str_appends(&buf_tmp, pprefix, result->variable,
                       "->channels_min : 1337, ", NULL);

        sp_str_appends(&buf_tmp, pprefix, result->variable, " ? ", NULL);
        sp_str_appends(&buf_tmp, pprefix, result->variable,
                       "->channels_max : 1337, ", NULL);

        sp_str_appends(&buf_tmp, pprefix, result->variable, " ? ", NULL);
        sp_str_appends(&buf_tmp, pprefix, result->variable,
                       "->buffer_bytes_max : 1337, ", NULL);

        sp_str_appends(&buf_tmp, pprefix, result->variable, " ? ", NULL);
        sp_str_appends(&buf_tmp, pprefix, result->variable,
                       "->period_bytes_min : 1337, ", NULL);

        sp_str_appends(&buf_tmp, pprefix, result->variable, " ? ", NULL);
        sp_str_appends(&buf_tmp, pprefix, result->variable,
                       "->period_bytes_max : 1337, ", NULL);

        sp_str_appends(&buf_tmp, pprefix, result->variable, " ? ", NULL);
        sp_str_appends(&buf_tmp, pprefix, result->variable,
                       "->periods_min : 1337, ", NULL);

        sp_str_appends(&buf_tmp, pprefix, result->variable, " ? ", NULL);
        sp_str_appends(&buf_tmp, pprefix, result->variable,
                       "->periods_max : 1337, ", NULL);

        sp_str_appends(&buf_tmp, pprefix, result->variable, " ? ", NULL);
        sp_str_appends(&buf_tmp, pprefix, result->variable,
                       "->fifo_size : 1337", NULL);
      } else {
        assert(false);
      }
      free(result->complex_raw);
      result->complex_raw    = strdup(sp_str_c_str(&buf_tmp));
      result->complex_printf = true;
      sp_str_free(&buf_tmp);
    } else if (strcmp(result->type, "snd_pcm_ops") == 0) {
#if 0
struct snd_pcm_ops {
  int (*open)(struct snd_pcm_substream *substream);
  int (*close)(struct snd_pcm_substream *substream);
  int (*ioctl)(struct snd_pcm_substream * substream, unsigned int cmd, void *arg);
  int (*hw_params)(struct snd_pcm_substream *substream, struct snd_pcm_hw_params *params);
  int (*hw_free)(struct snd_pcm_substream *substream);
  int (*prepare)(struct snd_pcm_substream *substream);
  int (*trigger)(struct snd_pcm_substream *substream, int cmd);
  snd_pcm_uframes_t (*pointer)(struct snd_pcm_substream *substream);
  int (*get_time_info)(struct snd_pcm_substream *substream, struct timespec *system_ts, struct timespec *audio_ts, struct snd_pcm_audio_tstamp_config *audio_tstamp_config, struct snd_pcm_audio_tstamp_report *audio_tstamp_report);
  int (*copy)(struct snd_pcm_substream *substream, int channel, snd_pcm_uframes_t pos, void __user *buf, snd_pcm_uframes_t count);
  int (*silence)(struct snd_pcm_substream *substream, int channel, snd_pcm_uframes_t pos, snd_pcm_uframes_t count); struct page *(*page)(struct snd_pcm_substream *substream, unsigned long offset);
  int (*mmap)(struct snd_pcm_substream *substream, struct vm_area_struct *vma);
  int (*ack)(struct snd_pcm_substream *substream);
};
#endif
      result->format = "open[%pF]close[%pF]ioctl[%pF]";
      sp_str buf_tmp;
      sp_str_init(&buf_tmp, 0);
      if (result->pointer) {
        sp_str_appends(&buf_tmp, pprefix, result->variable, " ? ", NULL);
        sp_str_appends(&buf_tmp, pprefix, result->variable, "->open : NULL, ",
                       NULL);

        sp_str_appends(&buf_tmp, pprefix, result->variable, " ? ", NULL);
        sp_str_appends(&buf_tmp, pprefix, result->variable, "->close : NULL, ",
                       NULL);

        sp_str_appends(&buf_tmp, pprefix, result->variable, " ? ", NULL);
        sp_str_appends(&buf_tmp, pprefix, result->variable, "->ioctl : NULL",
                       NULL);
      } else {
        assert(false);
      }
      free(result->complex_raw);
      result->complex_raw    = strdup(sp_str_c_str(&buf_tmp));
      result->complex_printf = true;
      sp_str_free(&buf_tmp);
    } else if (strcmp(result->type, "snd_pcm_ops") == 0) {
#if 0
struct snd_soc_card {
	const char *name;
	const char *long_name;
	const char *driver_name;
	struct device *dev;
	struct snd_card *snd_card;
	struct module *owner;
	struct mutex mutex;
	struct mutex dapm_mutex;
	bool instantiated;
	int (*probe)(struct snd_soc_card *card);
	int (*late_probe)(struct snd_soc_card *card);
	int (*remove)(struct snd_soc_card *card);
#endif
      result->format = "name[%s]long_name[%s]driver_name[%s]dev[%p]snd_card[%p]"
                       "probe[%pF]late_probe[%pF]";
      sp_str buf_tmp;
      sp_str_init(&buf_tmp, 0);
      if (result->pointer) {
        sp_str_appends(&buf_tmp, pprefix, result->variable, " ? ", NULL);
        sp_str_appends(&buf_tmp, pprefix, result->variable, "->name : \"\", ",
                       NULL);

        sp_str_appends(&buf_tmp, pprefix, result->variable, " ? ", NULL);
        sp_str_appends(&buf_tmp, pprefix, result->variable,
                       "->long_name : \"\", ", NULL);

        sp_str_appends(&buf_tmp, pprefix, result->variable, " ? ", NULL);
        sp_str_appends(&buf_tmp, pprefix, result->variable,
                       "->driver_name : \"\", ", NULL);

        sp_str_appends(&buf_tmp, pprefix, result->variable, " ? ", NULL);
        sp_str_appends(&buf_tmp, pprefix, result->variable, "->dev : NULL, ",
                       NULL);

        sp_str_appends(&buf_tmp, pprefix, result->variable, " ? ", NULL);
        sp_str_appends(&buf_tmp, pprefix, result->variable,
                       "->snd_card : NULL, ", NULL);

        sp_str_appends(&buf_tmp, pprefix, result->variable, " ? ", NULL);
        sp_str_appends(&buf_tmp, pprefix, result->variable, "->probe : NULL, ",
                       NULL);

        sp_str_appends(&buf_tmp, pprefix, result->variable, " ? ", NULL);
        sp_str_appends(&buf_tmp, pprefix, result->variable,
                       "->late_probe : NULL", NULL);
      } else {
        assert(false);
      }
      free(result->complex_raw);
      result->complex_raw    = strdup(sp_str_c_str(&buf_tmp));
      result->complex_printf = true;
      sp_str_free(&buf_tmp);
    } else if (strcmp(result->type, "snd_card") == 0) {
#if 0
struct snd_card {
  int number;			/* number of soundcard (index to snd_cards) */
  char id[16];			/* id string of this card */
  char driver[16];		/* driver name */
  char shortname[32];		/* short name of this soundcard */
  char longname[80];		/* name of this soundcard */
  char irq_descr[32];		/* Interrupt description */
  char mixername[80];		/* mixer name */
  char components[128];		/* card components delimited with space */
  struct module *module;		/* top-level module */
  void *private_data;		/* private data for soundcard */
  void (*private_free) (struct snd_card *card); /* callback for freeing of private data */
  struct list_head devices;	/* devices */
  struct device ctl_dev;		/* control device */
  unsigned int last_numid;	/* last used numeric ID */
  struct rw_semaphore controls_rwsem;	/* controls list lock */
  rwlock_t ctl_files_rwlock;	/* ctl_files list lock */
  int controls_count;		/* count of all controls */
  int user_ctl_count;		/* count of all user controls */
  struct list_head controls;	/* all controls for this card */
  struct list_head ctl_files;	/* active control files */
  struct mutex user_ctl_lock;	/* protects user controls against concurrent access */
  struct snd_info_entry *proc_root;	/* root for soundcard specific files */
  struct snd_info_entry *proc_id;	/* the card id */
  struct proc_dir_entry *proc_root_link;	/* number link to real id */
  struct list_head files_list;	/* all files associated to this card */
  struct snd_shutdown_f_ops *s_f_ops; /* file operations in the shutdown state */
  spinlock_t files_lock;		/* lock the files for this card */
  int shutdown;			/* this card is going down */
  struct completion *release_completion;
  struct device *dev;		/* device assigned to this card */
  struct device card_dev;		/* cardX object for sysfs */
};
#endif
      result->format = "number[%d]id[%s]driver[%s]shortname[%s]longname[%s]irq_"
                       "descr[%s]mixername[%s]components[%s]dev[%p]";
      sp_str buf_tmp;
      sp_str_init(&buf_tmp, 0);
      if (result->pointer) {
        sp_str_appends(&buf_tmp, pprefix, result->variable, " ? ", NULL);
        sp_str_appends(&buf_tmp, pprefix, result->variable, "->number : 1337, ",
                       NULL);

        sp_str_appends(&buf_tmp, pprefix, result->variable, " ? ", NULL);
        sp_str_appends(&buf_tmp, pprefix, result->variable, "->id : \"\", ",
                       NULL);

        sp_str_appends(&buf_tmp, pprefix, result->variable, " ? ", NULL);
        sp_str_appends(&buf_tmp, pprefix, result->variable, "->driver : \"\", ",
                       NULL);

        sp_str_appends(&buf_tmp, pprefix, result->variable, " ? ", NULL);
        sp_str_appends(&buf_tmp, pprefix, result->variable,
                       "->shortname : \"\", ", NULL);

        sp_str_appends(&buf_tmp, pprefix, result->variable, " ? ", NULL);
        sp_str_appends(&buf_tmp, pprefix, result->variable,
                       "->longname : \"\", ", NULL);
        sp_str_appends(&buf_tmp, pprefix, result->variable, " ? ", NULL);
        sp_str_appends(&buf_tmp, pprefix, result->variable,
                       "->irq_descr : \"\", ", NULL);
        sp_str_appends(&buf_tmp, pprefix, result->variable, " ? ", NULL);
        sp_str_appends(&buf_tmp, pprefix, result->variable,
                       "->mixername : \"\", ", NULL);
        sp_str_appends(&buf_tmp, pprefix, result->variable, " ? ", NULL);
        sp_str_appends(&buf_tmp, pprefix, result->variable,
                       "->components : \"\", ", NULL);

        sp_str_appends(&buf_tmp, pprefix, result->variable, " ? ", NULL);
        sp_str_appends(&buf_tmp, pprefix, result->variable, "->dev : NULL",
                       NULL);
      } else {
        assert(false);
      }
      free(result->complex_raw);
      result->complex_raw    = strdup(sp_str_c_str(&buf_tmp));
      result->complex_printf = true;
      sp_str_free(&buf_tmp);
    } else if (strcmp(result->type, "snd_soc_component") == 0) {
#if 0
struct snd_soc_component {
  const char *name;
  int id;
  const char *name_prefix;
  struct device *dev;
  struct snd_soc_card *card;
  unsigned int active;
  unsigned int ignore_pmdown_time:1; /* pmdown_time is ignored at stop */
  unsigned int registered_as_component:1;
  unsigned int auxiliary:1; /* for auxiliary component of the card */
  unsigned int suspended:1; /* is in suspend PM state */
  struct list_head list;
  struct list_head card_aux_list; /* for auxiliary bound components */
  struct list_head card_list;
  struct snd_soc_dai_driver *dai_drv;
  int num_dai;
  const struct snd_soc_component_driver *driver;
  struct list_head dai_list;
  int (*read)(struct snd_soc_component *, unsigned int, unsigned int *);
  int (*write)(struct snd_soc_component *, unsigned int, unsigned int);
};
#endif
      result->format =
        "name[%s]id[%d]name_prefix[%s]dev[%p]card[%p]read[%pF]write[%pF]";
      sp_str buf_tmp;
      sp_str_init(&buf_tmp, 0);
      if (result->pointer) {
        sp_str_appends(&buf_tmp, pprefix, result->variable, " ? ", NULL);
        sp_str_appends(&buf_tmp, pprefix, result->variable, "->name : \"\", ",
                       NULL);

        sp_str_appends(&buf_tmp, pprefix, result->variable, " ? ", NULL);
        sp_str_appends(&buf_tmp, pprefix, result->variable, "->id : 1337, ",
                       NULL);

        sp_str_appends(&buf_tmp, pprefix, result->variable, " ? ", NULL);
        sp_str_appends(&buf_tmp, pprefix, result->variable,
                       "->name_prefix : \"\", ", NULL);

        sp_str_appends(&buf_tmp, pprefix, result->variable, " ? ", NULL);
        sp_str_appends(&buf_tmp, pprefix, result->variable, "->dev : NULL, ",
                       NULL);

        sp_str_appends(&buf_tmp, pprefix, result->variable, " ? ", NULL);
        sp_str_appends(&buf_tmp, pprefix, result->variable, "->card : NULL, ",
                       NULL);

        sp_str_appends(&buf_tmp, pprefix, result->variable, " ? ", NULL);
        sp_str_appends(&buf_tmp, pprefix, result->variable, "->read : NULL, ",
                       NULL);

        sp_str_appends(&buf_tmp, pprefix, result->variable, " ? ", NULL);
        sp_str_appends(&buf_tmp, pprefix, result->variable, "->write : NULL",
                       NULL);
      } else {
        assert(false);
      }
      free(result->complex_raw);
      result->complex_raw    = strdup(sp_str_c_str(&buf_tmp));
      result->complex_printf = true;
      sp_str_free(&buf_tmp);

    } else if (strcmp(result->type, "snd_soc_platform") == 0) {
#if 0
struct snd_soc_platform {
  struct device *dev;
  const struct snd_soc_platform_driver *driver;
  struct list_head list;
  struct snd_soc_component component;
};
#endif
      result->format = "dev[%p]driver[%p]component[%p]";
      sp_str buf_tmp;
      sp_str_init(&buf_tmp, 0);
      if (result->pointer) {
        sp_str_appends(&buf_tmp, pprefix, result->variable, " ? ", NULL);
        sp_str_appends(&buf_tmp, pprefix, result->variable, "->dev : NULL, ",
                       NULL);

        sp_str_appends(&buf_tmp, pprefix, result->variable, " ? ", NULL);
        sp_str_appends(&buf_tmp, pprefix, result->variable, "->driver : NULL, ",
                       NULL);

        sp_str_appends(&buf_tmp, pprefix, result->variable, " ? &", NULL);
        sp_str_appends(&buf_tmp, pprefix, result->variable,
                       "->component : NULL", NULL);
      } else {
        assert(false);
      }
      free(result->complex_raw);
      result->complex_raw    = strdup(sp_str_c_str(&buf_tmp));
      result->complex_printf = true;
      sp_str_free(&buf_tmp);

    } else if (strcmp(result->type, "snd_soc_platform_driver") == 0) {
#if 0
struct snd_soc_platform_driver {
  int (*probe)(struct snd_soc_platform *);
  int (*remove)(struct snd_soc_platform *);
  struct snd_soc_component_driver component_driver;
  /* pcm creation and destruction */
  int (*pcm_new)(struct snd_soc_pcm_runtime *);
  void (*pcm_free)(struct snd_pcm *);
};
#endif
      result->format =
        "probe[%pF]remove[%pF]component_driver[%pF]pcm_new[%pF]pcm_free[%pF]";
      sp_str buf_tmp;
      sp_str_init(&buf_tmp, 0);
      if (result->pointer) {
        sp_str_appends(&buf_tmp, pprefix, result->variable, " ? ", NULL);
        sp_str_appends(&buf_tmp, pprefix, result->variable, "->probe : NULL, ",
                       NULL);

        sp_str_appends(&buf_tmp, pprefix, result->variable, " ? ", NULL);
        sp_str_appends(&buf_tmp, pprefix, result->variable, "->remove : NULL, ",
                       NULL);

        sp_str_appends(&buf_tmp, pprefix, result->variable, " ? &", NULL);
        sp_str_appends(&buf_tmp, pprefix, result->variable,
                       "->component_driver : NULL, ", NULL);

        sp_str_appends(&buf_tmp, pprefix, result->variable, " ? ", NULL);
        sp_str_appends(&buf_tmp, pprefix, result->variable,
                       "->pcm_new : NULL, ", NULL);

        sp_str_appends(&buf_tmp, pprefix, result->variable, " ? ", NULL);
        sp_str_appends(&buf_tmp, pprefix, result->variable, "->pcm_free : NULL",
                       NULL);
      } else {
        assert(false);
      }
      free(result->complex_raw);
      result->complex_raw    = strdup(sp_str_c_str(&buf_tmp));
      result->complex_printf = true;
      sp_str_free(&buf_tmp);
    } else if (strcmp(result->type, "snd_soc_component_driver") == 0) {
#if 0
struct snd_soc_component_driver {
  const char *name;
  const struct snd_kcontrol_new *controls;
  unsigned int num_controls;
  const struct snd_soc_dapm_widget *dapm_widgets;
  unsigned int num_dapm_widgets;
  const struct snd_soc_dapm_route *dapm_routes;
  unsigned int num_dapm_routes;
  int (*probe)(struct snd_soc_component *);
  void (*remove)(struct snd_soc_component *)
};
#endif
      result->format = "name[%s]probe[%pF]remove[%pF]";
      sp_str buf_tmp;
      sp_str_init(&buf_tmp, 0);
      if (result->pointer) {
        sp_str_appends(&buf_tmp, pprefix, result->variable, " ? ", NULL);
        sp_str_appends(&buf_tmp, pprefix, result->variable, "->name : \"\", ",
                       NULL);

        sp_str_appends(&buf_tmp, pprefix, result->variable, " ? ", NULL);
        sp_str_appends(&buf_tmp, pprefix, result->variable, "->probe : NULL, ",
                       NULL);

        sp_str_appends(&buf_tmp, pprefix, result->variable, " ? ", NULL);
        sp_str_appends(&buf_tmp, pprefix, result->variable, "->remove : NULL",
                       NULL);
      } else {
        assert(false);
      }
      free(result->complex_raw);
      result->complex_raw    = strdup(sp_str_c_str(&buf_tmp));
      result->complex_printf = true;
      sp_str_free(&buf_tmp);
    } else if (strcmp(result->type, "snd_soc_pcm_runtime") == 0) {
#if 0
struct snd_soc_pcm_runtime {
  struct device *dev;
  struct snd_soc_card *card;
  struct snd_soc_dai_link *dai_link;
  struct mutex pcm_mutex;
  enum snd_soc_pcm_subclass pcm_subclass;
  struct snd_pcm_ops ops;
  unsigned int dev_registered:1;
  /* Dynamic PCM BE runtime data */
  struct snd_soc_dpcm_runtime dpcm[2];
  int fe_compr;
  long pmdown_time;
  unsigned char pop_wait:1;
  /* runtime devices */
  struct snd_pcm *pcm;
  struct snd_compr *compr;
  struct snd_soc_codec *codec;
  struct snd_soc_platform *platform;
  struct snd_soc_dai *codec_dai;
  struct snd_soc_dai *cpu_dai;
  struct snd_soc_component *component; /* Only valid for AUX dev rtds */
};
#endif
      result->format = "dev[%p]card[%p]pcm[%p]";
      sp_str buf_tmp;
      sp_str_init(&buf_tmp, 0);
      if (result->pointer) {
        sp_str_appends(&buf_tmp, pprefix, result->variable, " ? ", NULL);
        sp_str_appends(&buf_tmp, pprefix, result->variable, "->dev : NULL, ",
                       NULL);

        sp_str_appends(&buf_tmp, pprefix, result->variable, " ? ", NULL);
        sp_str_appends(&buf_tmp, pprefix, result->variable, "->card : NULL, ",
                       NULL);

        sp_str_appends(&buf_tmp, pprefix, result->variable, " ? ", NULL);
        sp_str_appends(&buf_tmp, pprefix, result->variable, "->pcm : NULL",
                       NULL);
      } else {
        assert(false);
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
        result->format = "%p";
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
               strcmp(result->type, "long int") == 0 || //
               strcmp(result->type, "snd_pcm_sframes_t") == 0 || //
               strcmp(result->type, "signed long") == 0) {
      __format_numeric(result, pprefix, "%ld");
    } else if (strcmp(result->type, "unsigned long int") == 0 || //
               strcmp(result->type, "long unsigned int") == 0 || //
               strcmp(result->type, "snd_pcm_uframes_t") == 0 || //
               strcmp(result->type, "unsigned long") == 0 || //
               strcmp(result->type, "gulong") == 0) {
      __format_numeric(result, pprefix, "%lu");
    } else if (strcmp(result->type, "long long") == 0 || //
               strcmp(result->type, "long long int") == 0) {
      __format_numeric(result, pprefix, "%lld");
    } else if (strcmp(result->type, "unsigned long long") == 0 || //
               strcmp(result->type, "unsigned long long int") == 0 ||
               strcmp(result->type, "u64") == 0) {
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
    } else if (strcmp(result->type, "gint64") == 0) {
      result->format = "%\"G_GINT64_FORMAT\"";
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
    } else if (__format_libxml2(ctx, result, pprefix)) {
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

  /* fprintf(stderr, "%s: {\n", __func__); */
  if ((result = __field_name(ctx, subject, "identifier"))) {
    struct arg_list *it = result;
    while (it) {
      /* fprintf(stderr, "|%s\n", it->variable); */
      it->type = __field_type(ctx, subject, it, "");
      /* fprintf(stderr, "|%s: %s\n", it->type, it->variable); */
      __format(ctx, it, "");
      if (it->format && it->variable) {
        it->complete = true;
      }
      it = it->next;
    }
  }
  /* fprintf(stderr, "%s: }\n", __func__); */

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
  } else if (ctx->domain == LOG_ERR_DOMAIN) {
    sp_str_append(&buf, "  log_err(");
  } else if (ctx->domain == SYSLOG_DOMAIN) {
    sp_str_append(&buf, "  syslog(LOG_ERR, ");
  } else if (ctx->domain == F_ERROR_DOMAIN) {
    sp_str_append(&buf, "  f_error(");
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

  print_json_response(ctx->output_line, sp_str_c_str(&buf));

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

  /* printf("%s\n", sp_struct_value(ctx, subject)); */
  /* debug_subtypes_rec(ctx, subject, 0); */
  /* printf("here!"); */
  tmp = find_rec_chld_by_type(subject, "function_declarator");
  if (!ts_node_is_null(tmp)) {
    /* printf("%s:1\n", __func__); */
    tmp = find_direct_chld_by_type(tmp, "parameter_list");
    if (!ts_node_is_null(tmp)) {
      uint32_t i;
      struct arg_list *arg = NULL;
      /* printf("%s:4\n", __func__); */

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
            while (field_it->next) {
              field_it = field_it->next;
            }
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
    /* debug_subtypes_rec(ctx, it, 0); */
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
          while (field_it->next) {
            field_it = field_it->next;
          }
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
    struct arg_list *it = result;
    /* fprintf(stderr,"%s\n", result->variable); */
    result->type = __field_type(ctx, subject, result, pprefix);
    /* fprintf(stderr, "type[%s]\n", type); */

    while (it) {
      __format(ctx, it, pprefix);
      it->complete = false;
      if (!it->dead && it->format && it->variable) {
        it->complete = true;
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
  sp_str_appends(&buf, "}\", (void*)", pprefix, NULL);

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

  print_json_response(ctx->output_line, sp_str_c_str(&buf));

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

struct branch_list;
struct branch_list {
  struct branch_list *next;
  char *context;
  uint32_t line;
  uint32_t depth;
};
static struct branch_list *
new_branch_list(const char *context,
                uint32_t branch_id,
                uint32_t line,
                uint32_t depth)
{
  struct branch_list *result;
  char new_context_tmp[128] = {'\0'};
  char *new_context         = NULL;

  if (strlen(context) > 0) {
    sprintf(new_context_tmp, "%s.%d", context, branch_id);
  } else {
    sprintf(new_context_tmp, "%d", branch_id);
  }
  new_context = strdup(new_context_tmp);

  result  = calloc(1, sizeof(*result));
  *result = (struct branch_list){
    .next    = NULL,
    .context = new_context,
    .line    = line,
    .depth   = depth,
  };

  return result;
}
static bool
sp_branches_rec(struct sp_ts_Context *ctx,
                TSNode subject,
                struct branch_list *branches,
                const char *context,
                uint32_t branch_id,
                uint32_t depth);

static bool
sp_branches_compound_statement_rec(struct sp_ts_Context *ctx,
                                   TSNode subject,
                                   struct branch_list *branches,
                                   const char *context,
                                   uint32_t *branch_id,
                                   uint32_t depth)
{
  if (ts_node_child_count(subject) >= 1) {
    TSNode open_bracket           = ts_node_child(subject, 0);
    TSPoint open_bracket_point    = ts_node_start_point(open_bracket);
    const char *open_bracket_type = ts_node_type(open_bracket);

    if (strcmp(open_bracket_type, "{") != 0) {
      fprintf(stderr, "%s:open_bracket_type[%s]\n", __func__,
              open_bracket_type);
      exit(1); //BUG
      return false;
    }

    ++depth;
    branches = branches->next =
      new_branch_list(context, *branch_id, open_bracket_point.row + 1, depth);
    ++(*branch_id);

    sp_branches_rec(ctx, subject, branches, branches->context, 0, depth);
    while (branches->next) {
      branches = branches->next;
    }
  }

  return true;
}

static bool
sp_branches_if_statement_rec(struct sp_ts_Context *ctx,
                             TSNode subject,
                             struct branch_list *branches,
                             const char *context,
                             uint32_t *branch_id,
                             uint32_t depth)
{
  uint32_t i = 0;

  bool found_else = false;
  for (i = 0; i < ts_node_child_count(subject); ++i) {
    TSNode child           = ts_node_child(subject, i);
    const char *child_type = ts_node_type(child);

    if (strcmp(child_type, "compound_statement") == 0) {
      sp_branches_compound_statement_rec(ctx, child, branches, context,
                                         branch_id, depth);
      while (branches->next) {
        branches = branches->next;
      }

      if (found_else) {
        break;
      }
    } else if (strcmp(child_type, "if_statement") == 0) {
      sp_branches_if_statement_rec(ctx, child, branches, context, branch_id,
                                   depth);
      while (branches->next) {
        branches = branches->next;
      }
      break;
    } else if (strcmp(child_type, "else") == 0) {
      found_else = true;
    }
  } //for
  return true;
}

static bool
sp_branches_rec(struct sp_ts_Context *ctx,
                TSNode subject,
                struct branch_list *branches,
                const char *context,
                uint32_t branch_id,
                uint32_t depth)
{
  uint32_t i = 0;

  for (i = 0; i < ts_node_child_count(subject); ++i) {
    TSNode child           = ts_node_child(subject, i);
    const char *child_type = ts_node_type(child);

    if (strcmp(child_type, "if_statement") == 0) {
      sp_branches_if_statement_rec(ctx, child, branches, context, &branch_id,
                                   depth);
      while (branches->next) {
        branches = branches->next;
      }
    } else if (strcmp(child_type, "return_statement") == 0) {
      TSPoint point = ts_node_start_point(child);

      assert(!branches->next);
      branches = branches->next =
        new_branch_list(context, branch_id, point.row, depth);
    } else {
      sp_branches_rec(ctx, child, branches, context, branch_id, depth);
      while (branches->next) {
        branches = branches->next;
      }
    }
  } //for

  return true;
}

static int
sp_print_branches(struct sp_ts_Context *ctx, TSNode subject)
{
  struct branch_list dummy = {0};
  struct branch_list *it;
  TSNode body;
  sp_str buf;
  sp_str_init(&buf, 0);

  body = find_direct_chld_by_type(subject, "compound_statement");
  if (!ts_node_is_null(body)) {
    debug_subtypes_rec(ctx, body, 0);
    fprintf(stderr, "\n");
    /* printf("%s:\n", __func__); */
    /* debug_subtypes_rec(ctx, body, 0); */
    if (!sp_branches_rec(ctx, body, &dummy, "", 0, 1)) {
      assert(!dummy.next);
    }
  }

  {
    uint32_t len =
      0; // since by adding a line above we alter what line we should insert next
    json_t *root = json_object();
    {
      json_t *json_inserts = json_array();
      for (it = dummy.next; it; it = it->next) {
        json_t *json_insert = json_object();
        {
          uint32_t i;

          for (i = 0; i < it->depth; ++i) {
            sp_str_append(&buf, "  ");
          }

          if (ctx->domain == DEFAULT_DOMAIN) {
            sp_str_append(&buf, "printf(");
          } else if (ctx->domain == LOG_ERR_DOMAIN) {
            sp_str_append(&buf, "  log_err(");
          } else if (ctx->domain == SYSLOG_DOMAIN) {
            sp_str_append(&buf, "syslog(LOG_ERR, ");
          } else if (ctx->domain == F_ERROR_DOMAIN) {
            sp_str_append(&buf, "  f_error(");
          } else if (ctx->domain == LINUX_KERNEL_DOMAIN) {
            sp_str_append(&buf, "printk(KERN_ERR ");
          }

          sp_str_appends(&buf, "\"%s:", it->context, NULL);
          sp_str_append(&buf, "\\n\", __func__);");
          json_object_set_new(json_insert, "data",
                              json_string(sp_str_c_str(&buf)));
        }
        json_object_set_new(json_insert, "line", json_integer(it->line + len));
        json_array_append_new(json_inserts, json_insert);
        ++len;

        sp_str_clear(&buf);
      }
      json_object_set_new(root, "inserts", json_inserts);
    }

    {
      char *json_response;
      json_response = json_dumps(root, JSON_PRESERVE_ORDER);
      fprintf(stdout, "%s", json_response);
      fflush(stdout);
      free(json_response);
    }

    json_decref(root);
  }

  sp_str_free(&buf);
  return EXIT_SUCCESS;
}

int
main(int argc, const char *argv[])
{
  int res                  = EXIT_FAILURE;
  struct sp_ts_Context ctx = {0};
  const char *in_type      = NULL;
  const char *in_file      = NULL;
  const char *in_line      = NULL;
  const char *in_column    = NULL;
  TSPoint pos              = {0};

  if (argc != 5) {
    if (argc > 1) {
      in_type = argv[1];
      if (argc == 3 && strcmp(in_type, "print") == 0) {
        in_file = argv[2];
        return main_print(in_file);
      }
    }
    fprintf(stderr, "%s crunch|line|print|branches file line column\n",
            argv[0]);
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
  ctx.output_line = pos.row + 1;

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
            const char *fun_def     = "function_definition";
            TSNode found = sp_find_parent(highligted, struct_spec, enum_spec,
                                          fun_def, class_spec);
            if (!ts_node_is_null(found)) {
              if (strcmp(ts_node_type(found), struct_spec) == 0) {
                if (strcmp(in_type, "crunch") == 0) {
                  ctx.output_line = sp_find_last_line(found);
                  res             = sp_print_struct(&ctx, found);
                }
              }
#if 1
              else if (strcmp(ts_node_type(found), class_spec) == 0) {
                if (strcmp(in_type, "crunch") == 0) {
                  ctx.output_line = sp_find_last_line(found);
                  res             = sp_print_class(&ctx, found);
                }
              }
#endif
              else if (strcmp(ts_node_type(found), enum_spec) == 0) {

                if (strcmp(in_type, "crunch") == 0) {
                  ctx.output_line = sp_find_last_line(found);
                  res             = sp_print_enum(&ctx, found);
                }
              } else if (strcmp(ts_node_type(found), fun_def) == 0) {
                if (strcmp(in_type, "crunch") == 0) {
                  /* printf("%s\n", ts_node_string(found)); */
                  ctx.output_line = sp_find_open_bracket(found);
                  res             = sp_print_function_args(&ctx, found);
                } else if (strcmp(in_type, "branches") == 0) {
                  res = sp_print_branches(&ctx, found);
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

//TODO when we make assumption example (unsigned char*xxx, size_t l_xxx) make a comment in the debug function
// example: NOTE: assumes xxx and l_xxx is related

// TODO generate 2 print function typedef struct name {} name_t;
//
// TODO when leader+m try to paste after all variable inits
//
// TODO enum bitset FIRST=1 SECOND=2 THRID=4, var=FIRST|SECOND
//      if enum has explicit values and if binary they do not owerlap = assume enum bitset
