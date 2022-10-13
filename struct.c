#define _GNU_SOURCE
#include <tree_sitter/api.h>

#include "shared.h"
#include "to_string.h"
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

extern const TSLanguage *
tree_sitter_c(void);

extern const TSLanguage *
tree_sitter_cpp(void);

static struct arg_list *
__field_to_arg(struct sp_ts_Context *ctx,
               TSNode subject,
               const char *pprefix,
               AccessSpecifier_t specifier);

static char *
sp_struct_value(struct sp_ts_Context *ctx, TSNode subject)
{
  uint32_t s   = ts_node_start_byte(subject);
  uint32_t e   = ts_node_end_byte(subject);
  char *it     = &ctx->file.content[s];
  uint32_t len = e - s;
  assert(e >= s);
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

  if (len == 0) {
    return NULL;
  }
  return strndup(it, len);
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
  if (strcasestr(file, "-linux-axis") != NULL)
    return LINUX_KERNEL_DOMAIN;
  if (strcasestr(file, "-modartpec") != NULL)
    return LINUX_KERNEL_DOMAIN;
  if (strcasestr(file, "-workspace-sources-ioboxd") != NULL ||
      strcasestr(file, "-workspace-sources-focusd") != NULL)
    return LOG_ERR_DOMAIN;
  if (strcasestr(file, "-eventbridge-plugins-propertychanged") != NULL)
    return F_ERROR_DOMAIN;
  if (strcasestr(file, "-libevent2") != NULL ||
      strcasestr(file, "-libconfiguration-event") != NULL)
    return AX_ERROR_DOMAIN;
  if (strcasestr(file, "-dists-") != NULL)
    return SYSLOG_DOMAIN;

  return DEFAULT_DOMAIN;
}

static TSNode
sp_find_parent0(TSNode subject, const char *needle0)
{

  TSNode it     = subject;
  TSNode result = {0};

  while (!ts_node_is_null(it)) {
    if (strcmp(ts_node_type(it), needle0) == 0) {
      result = it;
    }
    it = ts_node_parent(it);
  }
  return result;
}

static TSNode
sp_find_parent(TSNode subject,
               const char *needle0,
               const char *needle1,
               const char *needle2,
               const char *needle3,
               const char *needle4)
{
  TSNode it     = subject;
  TSNode result = {0};

  while (!ts_node_is_null(it)) {
    /* fprintf(stderr, "%s:%s\n", __func__, ts_node_type(it)); */
    if (strcmp(ts_node_type(it), needle0) == 0 ||
        strcmp(ts_node_type(it), needle1) == 0 ||
        strcmp(ts_node_type(it), needle2) == 0 ||
        strcmp(ts_node_type(it), needle3) == 0 ||
        strcmp(ts_node_type(it), needle4) == 0) {
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
                char *ref  = sp_struct_value(ctx, node1);
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
print_json_empty_response(void)
{
  json_t *root         = json_object();
  json_t *json_inserts = json_array();
  json_object_set_new(root, "inserts", json_inserts);

  {
    char *r = json_dumps(root, JSON_PRESERVE_ORDER);
    fprintf(stdout, "%s", r);
    fflush(stdout);
    free(r);
  }
  json_decref(root);
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
    char *r = json_dumps(root, JSON_PRESERVE_ORDER);
    fprintf(stdout, "%s", r);
    fflush(stdout);
    free(r);
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
scoped_type_identifier_Type(struct sp_ts_Context *ctx, TSNode subject)
{
  TSNode tmp;
  TSNode it = subject;
  while (1) {
    /* XXX store namespace  */
    tmp = find_direct_chld_by_type(it, "scoped_namespace_identifier");
    if (!ts_node_is_null(tmp)) {
      it = tmp;
    } else {
      break;
    }
  }

  tmp = find_direct_chld_by_type(subject, "type_identifier");
  if (!ts_node_is_null(tmp)) {
    return (sp_struct_value(ctx, tmp));
  }

  return NULL;
}

static void
__field_type(struct sp_ts_Context *ctx,
             TSNode subject,
             struct arg_list *result,
             const char *pprefix)
{
  TSNode tmp;
  /* fprintf(stderr, "%s:=========================\n", __func__); */

  tmp = find_direct_chld_by_type(subject, "primitive_type");
  if (!ts_node_is_null(tmp)) {
    /* $primitive_type $field_identifier; */
    result->type = sp_struct_value(ctx, tmp);
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
      result->type = strdup(sp_str_c_str(&tmp_str));
      /* fprintf(stderr, "%s:2 [%s]\n", __func__, type); */
      sp_str_free(&tmp_str);
    } else {
      tmp = find_direct_chld_by_type(subject, "type_identifier");
      if (!ts_node_is_null(tmp)) {
        TSNode err_t = find_direct_chld_by_type(subject, "ERROR");
        if (!ts_node_is_null(err_t)) {
          /* fprintf(stderr, "%s:\n", __func__); */
          /* debug_subtypes_rec(ctx, subject, 0); */
          tmp = find_direct_chld_by_type(err_t, "identifier");
          if (!ts_node_is_null(tmp)) {
            /* g_autofree gchar *var; */
            result->type = sp_struct_value(ctx, tmp);
            /* fprintf(stderr, "%s:type[%s]\n", __func__, type); */
          }
        } else {
          /* $type_identifier $field_identifier;
         * Example:
         *  type_t type0;
         *  gint int0;
         */
          result->type = sp_struct_value(ctx, tmp);
          /* fprintf(stderr, "%s:3 [%s]\n", __func__, type); */
        }
      } else {
        tmp = find_direct_chld_by_type(subject, "enum_specifier");
        if (!ts_node_is_null(tmp)) {
          TSNode type_id;

          type_id = find_direct_chld_by_type(tmp, "type_identifier");
          if (!ts_node_is_null(type_id)) {
            result->type = sp_struct_value(ctx, type_id);
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
              result->type = sp_struct_value(ctx, type_id);
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

                    if ((arg = __field_to_arg(ctx, field, "in->", AS_PUBLIC))) {
                      field_it->next = arg;
                      while (field_it->next) {
                        field_it = field_it->next;
                      }
                    }
                  }
                } //for
                result->rec = field_dummy.next;
              } else {
                fprintf(stderr, "5.2.2\n");
              }
            }
          } else {
            TSNode ns_id;
            ns_id = find_direct_chld_by_type(subject, "scoped_type_identifier");
            if (!ts_node_is_null(ns_id)) {
              result->type = scoped_type_identifier_Type(ctx, ns_id);
            } else {
              TSNode temp_t;
              temp_t = find_direct_chld_by_type(subject, "template_type");
              if (!ts_node_is_null(temp_t)) {
                //TODO store template arguments
                TSNode type_id;
                type_id = find_direct_chld_by_type(temp_t, "type_identifier");
                if (!ts_node_is_null(type_id)) {
                  result->type = sp_struct_value(ctx, type_id);
                } else {
                  TSNode ns_id2;
                  ns_id2 =
                    find_direct_chld_by_type(temp_t, "scoped_type_identifier");
                  if (!ts_node_is_null(ns_id2)) {
                    result->type = scoped_type_identifier_Type(ctx, ns_id2);
                  }
                }
              } else {
                tmp = find_direct_chld_by_type(subject, "macro_type_specifier");
                if (!ts_node_is_null(tmp)) {
                  TSNode macro_t = find_direct_chld_by_type(tmp, "identifier");
                  if (!ts_node_is_null(macro_t)) {
                    result->macro_type = sp_struct_value(ctx, tmp);
                  }

                  tmp = find_direct_chld_by_type(tmp, "type_descriptor");
                  if (!ts_node_is_null(tmp)) {
                    tmp = find_direct_chld_by_type(tmp, "type_identifier");
                    if (!ts_node_is_null(tmp)) {
                      /* g_autoptr(Type) var; */
                      result->type = sp_struct_value(ctx, tmp);
                    }
                  }
                  debug_subtypes_rec(ctx, tmp, 0);
                } else {
                  fprintf(stderr, "HERE\n");
                  debug_subtypes_rec(ctx, subject, 0);
                }
              }
            }
          }
        }
      }
    }
  }
}

static struct arg_list *
xx(struct sp_ts_Context *ctx,
   struct arg_list *result,
   TSNode subject,
   const char *id_type);

static struct arg_list *
__field_name(struct sp_ts_Context *ctx, TSNode subject, const char *id_type)
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
      __rec_search(ctx, ptr_decl, id_type, 1, &result->pointer);
    }
    xx(ctx, result, init_decl, id_type);
  } else {
    xx(ctx, result, subject, id_type);
  }

  /* fprintf(stderr, "%s:}\n", __func__); */
  return result;
}

static struct arg_list *
xx(struct sp_ts_Context *ctx,
   struct arg_list *result,
   TSNode subject,
   const char *id_type)
{
  /* fprintf(stderr, "  %s:{\n", __func__); */
  /* debug_subtypes_rec(ctx, subject, 1); */
  TSNode ptr_decl = find_direct_chld_by_type(subject, "pointer_declarator");
  if (!ts_node_is_null(ptr_decl)) {
    TSNode id_decl = find_rec_chld_by_type(subject, id_type);
    if (!ts_node_is_null(id_decl)) {
      struct arg_list *rit = result;
      if (rit->variable) {
        rit->next = calloc(1, sizeof(*rit));
        rit       = rit->next;
      }
      rit->variable = sp_struct_value(ctx, id_decl);
      __rec_search(ctx, ptr_decl, id_type, 1, &result->pointer);
    }
  } else {
    TSNode id_decl = find_direct_chld_by_type(subject, id_type);
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

        field_id = find_direct_chld_by_type(tmp, id_type);
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
              tmp = __rec_search(ctx, tmp, id_type, 1, &result->pointer);
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
            return __field_name(ctx, tmp, id_type);
          } else {
            tmp = find_direct_chld_by_type(subject, "reference_declarator");
            if (!ts_node_is_null(tmp)) {
              TSNode id_decl2 = find_rec_chld_by_type(subject, id_type);
              if (!ts_node_is_null(id_decl2)) {
                result->variable = sp_struct_value(ctx, id_decl2);
              }
            } else {
            }
          }
        }
      }
    }
  }
  /* fprintf(stderr, "  %s:}\n", __func__); */
  return result;
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
      __field_type(ctx, subject, it, "");
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
  bool trailing_newline = true;

  sp_str_init(&buf, 0);
  sp_str_init(&line_buf, 0);

  if (ctx->domain == DEFAULT_DOMAIN) {
    sp_str_append(&buf, "  fprintf(stderr, ");
  } else if (ctx->domain == LOG_ERR_DOMAIN) {
    sp_str_append(&buf, "  log_err(");
  } else if (ctx->domain == SYSLOG_DOMAIN) {
    sp_str_append(&buf, "  syslog(LOG_ERR, ");
  } else if (ctx->domain == F_ERROR_DOMAIN) {
    sp_str_append(&buf, "  f_error(");
  } else if (ctx->domain == AX_ERROR_DOMAIN) {
    sp_str_append(&buf, "  ax_error(");
    trailing_newline = false;
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
      fprintf(stderr, "%s: Incomplete: var:%s: type:%s\n", __func__,
              field_it->variable ? field_it->variable : "NULL",
              field_it->type ? field_it->type : "NULL");
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

  if (trailing_newline) {
    sp_str_append(&buf, "\\n");
  }
  sp_str_append(&buf, "\", __func__");
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
  int res                     = EXIT_SUCCESS;
  TSNode tmp                  = {0};
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
    fprintf(stderr,"- %s\n", ts_node_string(child));
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
      fprintf(stderr, "null\n");
    }

  } else {
    /* fprintf(stderr,"null\n"); */
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

  return res;
}

static struct arg_list *
__field_to_arg(struct sp_ts_Context *ctx,
               TSNode subject,
               const char *pprefix,
               AccessSpecifier_t specifier)
{
  struct arg_list *result = NULL;

  /* fprintf(stderr, "%s\n", __func__); */

  /* TODO result->format, result->variable strdup() */

  if ((result = __field_name(ctx, subject, "field_identifier"))) {
    struct arg_list *it = result;
    /* fprintf(stderr,"%s\n", result->variable); */
    __field_type(ctx, subject, result, pprefix);
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
sp_do_print_typedef(const char *type_name, const char *t_type_name, sp_str *buf)
{
  sp_str_appends(buf, "static inline const char* sp_debug_", t_type_name, "(",
                 NULL);
  sp_str_appends(buf, "const ", t_type_name, " *in", NULL);
  sp_str_append(buf, ") {\n");
  sp_str_appends(buf, "  return sp_debug_", type_name, "(in);\n", NULL);
  sp_str_append(buf, "}\n");
  return EXIT_SUCCESS;
}

static int
sp_print_typedef(struct sp_ts_Context *ctx, TSNode type_def)
{
  int res           = EXIT_FAILURE;
  sp_str buf        = {0};
  char *type_name   = NULL;
  char *t_type_name = NULL;
  TSNode struct_spec;
  TSNode tmp;
  /* debug_subtypes_rec(ctx, type_def, 0); */

  struct_spec = find_direct_chld_by_type(type_def, "struct_specifier");
  if (ts_node_is_null(tmp)) {
    goto Lerr;
  }

  tmp = find_direct_chld_by_type(struct_spec, "type_identifier");
  if (!ts_node_is_null(tmp)) {
    type_name = sp_struct_value(ctx, tmp);
  } else {
    goto Lerr;
  }

  tmp = find_direct_chld_by_type(type_def, "type_identifier");
  if (!ts_node_is_null(tmp)) {
    t_type_name = sp_struct_value(ctx, tmp);
  } else {
    goto Lerr;
  }

  sp_str_init(&buf, 0);

  sp_do_print_typedef(type_name, t_type_name, &buf);
  print_json_response(ctx->output_line, sp_str_c_str(&buf));

  res = EXIT_SUCCESS;
Lerr:
  free(t_type_name);
  free(type_name);
  sp_str_free(&buf);
  return res;
}

static int
sp_do_print_struct(struct sp_ts_Context *ctx,
                   const char *type_name,
                   const char *t_type_name,
                   struct arg_list *const fields,
                   const char *pprefix)
{
  struct arg_list *field_it;
  size_t complete = 0;
  sp_str buf;
  sp_str_init(&buf, 0);
  /* fprintf(stderr, */
  /*         "%s:type_name[%s]t_type_name[%s]" // */
  /*         "pprefix[%s]\n", */
  /*         __func__, type_name, t_type_name, pprefix); */

  (void)ctx;

  const char *def_type_name = type_name ? type_name : t_type_name;

  sp_str_appends(&buf, "static inline const char* sp_debug_", def_type_name,
                 "(", NULL);
  sp_str_appends(&buf, "const ", type_name ? "struct " : "", def_type_name,
                 " *", pprefix, NULL);
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
  sp_str_appends(&buf, "}\", (const void*)", pprefix, NULL);

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

  if (type_name && t_type_name) {
    sp_do_print_typedef(type_name, t_type_name, &buf);
  }

  print_json_response(ctx->output_line, sp_str_c_str(&buf));

  sp_str_free(&buf);
  return EXIT_SUCCESS;
}

static int
sp_print_struct(struct sp_ts_Context *ctx,
                TSNode subject,
                const char *t_type_name)
{
  int res                     = EXIT_FAILURE;
  char *type_name             = NULL;
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
  }

  if (!type_name && !t_type_name) {
    goto Lexit;
  }

  tmp = find_direct_chld_by_type(subject, "field_declaration_list");
  if (!ts_node_is_null(tmp)) {
    for (i = 0; i < ts_node_child_count(tmp); ++i) {
      TSNode field = ts_node_child(tmp, i);
      /* fprintf(stderr, "i.%u\n", i); */
      if (strcmp(ts_node_type(field), "field_declaration") == 0) {
        struct arg_list *arg = NULL;

        if ((arg = __field_to_arg(ctx, field, pprefix, AS_PUBLIC))) {
          field_it->next = arg;
          while (field_it->next) {
            field_it = field_it->next;
          }
        }
      }
    } //for
  }

  sp_do_print_struct(ctx, type_name, t_type_name, field_dummy.next, pprefix2);
  res = EXIT_SUCCESS;
Lexit:
  free(type_name);
  return res;
}

static int
sp_do_print_class(struct sp_ts_Context *ctx,
                  const char *type_name,
                  struct arg_list *const fields,
                  const char *pprefix,
                  uint32_t row)
{
  struct arg_list *field_it;
  size_t complete    = 0;
  const char *indent = "  ";
  sp_str buf;
  sp_str_init(&buf, 0);

  (void)ctx;

  sp_str_appends(&buf, indent, "public:\n", NULL);
  sp_str_appends(&buf, indent, "const char* sp_debug() const {\n", NULL);
  sp_str_appends(&buf, indent, "  static char buf[1024] = {'\\0'};\n", NULL);
  field_it = fields;
  sp_str_appends(&buf, indent, "  snprintf(buf, sizeof(buf), \"", type_name,
                 "{", NULL);
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
  sp_str_append(&buf, "}\"");

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
  sp_str_appends(&buf, indent, "  return buf;\n", NULL);
  sp_str_appends(&buf, indent, "}\n", NULL);

  print_json_response(row, sp_str_c_str(&buf));

  sp_str_free(&buf);
  return EXIT_SUCCESS;
}

static int
sp_print_class(struct sp_ts_Context *ctx, TSNode subject)
{
  int res = EXIT_FAILURE;
  uint32_t i;
  TSNode fdl;
  TSNode tmp;
  struct arg_list field_dummy = {0};
  struct arg_list *field_it   = &field_dummy;
  char *type_name             = NULL;
  AccessSpecifier_t specifier = AS_PACKAGE_PROTECTED;
  uint32_t row                = 0;

  const char *pprefix  = "this->";
  const char *pprefix2 = "this";

  tmp = find_direct_chld_by_type(subject, "type_identifier");
  if (!ts_node_is_null(tmp)) {
    /* class type_name { ... }; */
    type_name = sp_struct_value(ctx, tmp);
  }

  fdl = find_direct_chld_by_type(subject, "field_declaration_list");
  if (!ts_node_is_null(fdl)) {
    debug_subtypes_rec(ctx, fdl, 0);
    for (i = 0; i < ts_node_child_count(fdl); ++i) {
      TSNode child = ts_node_child(fdl, i);
      if (strcmp(ts_node_type(child), "field_declaration") == 0) {
        struct arg_list *arg = NULL;

        if ((arg = __field_to_arg(ctx, child, pprefix, specifier))) {
          field_it->next = arg;
          while (field_it->next) {
            field_it = field_it->next;
          }
        }
      } else if (strcmp(ts_node_type(child), "access_specifier") == 0) {
        if (!ts_node_is_null(find_direct_chld_by_type(subject, "private"))) {
          specifier = AS_PRIVATE;
          break;
        } else if (!ts_node_is_null(
                     find_direct_chld_by_type(subject, "public"))) {
          specifier = AS_PUBLIC;
          break;
        } else if (!ts_node_is_null(
                     find_direct_chld_by_type(subject, "protected"))) {
          specifier = AS_PROTECTED;
          break;
        }
      } else if (strcmp(ts_node_type(child), "}") == 0) {
        TSPoint p = ts_node_end_point(subject);
        row       = p.row;
      }
    }

    sp_do_print_class(ctx, type_name, field_dummy.next, pprefix2, row);
    res = EXIT_SUCCESS;
  }
  return res;
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
main_print(const char *in_file, int kind)
{
  int res                  = EXIT_FAILURE;
  struct sp_ts_Context ctx = {0};
  if (mmap_file(in_file, &ctx.file) == 0) {
    TSParser *parser = ts_parser_new();
    if (is_cpp_file(in_file)) {
      const TSLanguage *cpplang = tree_sitter_cpp();
      fprintf(stderr, "cpp\n");
      ts_parser_set_language(parser, cpplang);
    } else if (is_c_file(in_file)) {
      const TSLanguage *clang = tree_sitter_c();
      fprintf(stderr, "c\n");
      ts_parser_set_language(parser, clang);
    } else {
      const TSLanguage *clang = tree_sitter_c();
      fprintf(stderr, "unknown (c)\n");
      ts_parser_set_language(parser, clang);
    }

    ctx.tree = ts_parser_parse_string(parser, NULL, ctx.file.content,
                                      (uint32_t)ctx.file.length);
    if (!ctx.tree) {
      fprintf(stderr, "failed to parse\n");
      goto Lerr;
    }

    if (kind == 0) {
      TSNode root = ts_tree_root_node(ctx.tree);
      if (!ts_node_is_null(root)) {
        printf("%s\n", ts_node_string(root));
      } else {
        goto Lerr;
      }
    } else {
      TSNode root = ts_tree_root_node(ctx.tree);
      debug_subtypes_rec(&ctx, root, 0);
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
    // since by adding a line above we alter what line we should insert next
    uint32_t len = 0;
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

          bool trailing_newline = true;
          if (ctx->domain == DEFAULT_DOMAIN) {
            sp_str_append(&buf, "  fprintf(stderr, ");
          } else if (ctx->domain == LOG_ERR_DOMAIN) {
            sp_str_append(&buf, "  log_err(");
          } else if (ctx->domain == SYSLOG_DOMAIN) {
            sp_str_append(&buf, "syslog(LOG_ERR, ");
          } else if (ctx->domain == F_ERROR_DOMAIN) {
            sp_str_append(&buf, "  f_error(");
          } else if (ctx->domain == AX_ERROR_DOMAIN) {
            sp_str_append(&buf, "  ax_error(");
            trailing_newline = false;
          } else if (ctx->domain == LINUX_KERNEL_DOMAIN) {
            sp_str_append(&buf, "printk(KERN_ERR ");
          }

          sp_str_appends(&buf, "\"%s:", it->context, NULL);
          if (trailing_newline) {
            sp_str_append(&buf, "\\n");
          }
          sp_str_append(&buf, "\", __func__);");
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
        return main_print(in_file, 0);
      } else if (argc == 3 && strcmp(in_type, "print2") == 0) {
        in_file = argv[2];
        return main_print(in_file, 1);
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
    TSParser *parser = ts_parser_new();
    if (is_cpp_file(in_file)) {
      const TSLanguage *cpplang = tree_sitter_cpp();
      ts_parser_set_language(parser, cpplang);
    } else if (is_c_file(in_file)) {
      const TSLanguage *clang = tree_sitter_c();
      ts_parser_set_language(parser, clang);
    } else {
      const TSLanguage *clang = tree_sitter_c();
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
        highligted = ts_node_descendant_for_point_range(root, pos, pos);
        if (!ts_node_is_null(highligted)) {
          if (strcmp(in_type, "locals") == 0) {
            TSNode fun     = sp_find_parent0(highligted, "function_definition");
            TSPoint hpoint = ts_node_start_point(highligted);
            if (ts_node_is_null(fun)) {
              // we can only print locals inside a function
              print_json_empty_response();
              return EXIT_SUCCESS;
            }

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
            const char *struct_spec  = "struct_specifier";
            const char *typedef_spec = "type_definition";
            const char *class_spec   = "class_specifier";
            const char *enum_spec    = "enum_specifier";
            const char *fun_def      = "function_definition";
            TSNode found = sp_find_parent(highligted, struct_spec, typedef_spec,
                                          enum_spec, fun_def, class_spec);
            if (!ts_node_is_null(found)) {
              if (strcmp(ts_node_type(found), struct_spec) == 0) {
                if (strcmp(in_type, "crunch") == 0) {
                  TSNode tmp;
                  tmp =
                    find_direct_chld_by_type(found, "field_declaration_list");
                  if (!ts_node_is_null(tmp)) {
                    ctx.output_line = sp_find_last_line(found);
                    res             = sp_print_struct(&ctx, found, NULL);
                  } else {
                    ctx.output_line = sp_find_last_line(found);
                    TSNode type_def = ts_node_parent(found);
                    if (strcmp(ts_node_type(type_def), "type_definition") ==
                        0) {
                      res = sp_print_typedef(&ctx, type_def);
                      /* debug_subtypes_rec(&ctx, found, 0); */
                    }
                  }
                }
              } else if (strcmp(ts_node_type(found), typedef_spec) == 0) {
                TSNode tmp;
                tmp = find_rec_chld_by_type(found, "field_declaration_list");
                if (!ts_node_is_null(tmp)) {
                  char *t_type_name = NULL;
                  tmp = find_direct_chld_by_type(found, "type_identifier");
                  if (!ts_node_is_null(tmp)) {
                    /* typedef struct ... { ... } t_type_name; */
                    t_type_name = sp_struct_value(&ctx, tmp);
                  }
                  /* fprintf(stderr, "%s:t_type_name[%s]\n", __func__, */
                  /*         t_type_name); */
                  /* debug_subtypes_rec(&ctx, found, 0); */

                  tmp = find_direct_chld_by_type(found, struct_spec);
                  if (!ts_node_is_null(tmp)) {
                    ctx.output_line = sp_find_last_line(found);
                    res             = sp_print_struct(&ctx, tmp, t_type_name);
                  }

                  free(t_type_name);
                } else {
                  ctx.output_line = sp_find_last_line(found);
                  res             = sp_print_typedef(&ctx, found);
                }
              } else if (strcmp(ts_node_type(found), class_spec) == 0) {
                if (strcmp(in_type, "crunch") == 0) {
                  ctx.output_line = sp_find_last_line(found);
                  res             = sp_print_class(&ctx, found);
                }
              } else if (strcmp(ts_node_type(found), enum_spec) == 0) {

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
            } else {
              fprintf(stderr, "not inside a scope\n");
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
// TODO when leader+m try to paste after all variable inits

// TODO detect cycles
// struct dummy_list {
//   struct dummy_list *rec;
// };
// TODO c++ template arguments: vector<int>, map<int,int>
