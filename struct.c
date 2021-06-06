#include <tree_sitter/api.h>

#include "shared.h"

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
sp_find_parent(TSNode subject, const char *needle)
{
  TSNode it          = subject;
  const TSNode empty = {0};

  while (!ts_node_is_null(it)) {
    if (strcmp(ts_node_type(it), needle) == 0) {
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

static TSNode
sp_find_struct_specifier(TSNode subject, const struct sp_ts_file *file)
{
  TSNode result = {0};
  (void)file;

  result = sp_find_parent(subject, "struct_specifier");
  if (!ts_node_is_null(result)) {
    return result;
  }
  /* XXX: maybe downwards? */

  return result;
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
sp_print_struct(struct sp_ts_Context *ctx, TSNode subject)
{
  char *type_name        = NULL;
  bool type_name_typedef = false;
  uint32_t i;
  TSNode tmp;
  /*   uint32_t s   = ts_node_start_byte(subject); */
  /*   uint32_t e   = ts_node_end_byte(subject); */
  /*   uint32_t len = e - s; */
  /*   fprintf(stderr, "s: %u\n", s); */
  /*   fprintf(stderr, "e: %u\n", e); */
  /*   fprintf(stderr, "len: %u\n", len); */
  /*   fprintf(stderr, "%.*s: %s\n", (int)len, &ctx->file.content[s], */
  /*           ts_node_type(subject)); */
  /*   fprintf(stderr, "children: %u\n", ts_node_child_count(subject)); */

#if 0
  char *p = ts_node_string(subject);
  fprintf(stdout, "%s\n", p);
  free(p);
#endif

  /* try to find a name */
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
          type_name_typedef = true;
          type_name         = sp_struct_value(ctx, tmp);
        }
      }
    }
  }

#if 0
  for (i = 0; i < ts_node_child_count(subject); ++i) {
    uint32_t s   = ts_node_start_byte(ts_node_child(subject, i));
    uint32_t e   = ts_node_end_byte(ts_node_child(subject, i));
    uint32_t len = e - s;
    fprintf(stderr, ".%u\n", i);
    fprintf(stderr, "s: %u\n", s);
    fprintf(stderr, "e: %u\n", e);
    fprintf(stderr, "len: %u\n", len);
    fprintf(stderr, "%.*s: %s\n", (int)len, &ctx->file.content[s],
            ts_node_type(ts_node_child(subject, i)));
  }
#endif
  printf("type_name: %s\n", type_name ? type_name : "NULL");
}

int
main(int argc, const char *argv[])
{
  int res                  = EXIT_SUCCESS;
  TSPoint pos              = {.row = 0, .column = 0};
  struct sp_ts_Context ctx = {0};

  if (argc != 4) {
    fprintf(stderr, "%s file line column\n", argv[0]);
    return EXIT_FAILURE;
  }

  if (!sp_parse_uint32_t(argv[2], &pos.row)) {
    fprintf(stderr, "failed to parse line '%s'\n", argv[2]);
    return EXIT_FAILURE;
  }
  if (!sp_parse_uint32_t(argv[3], &pos.column)) {
    fprintf(stderr, "failed to parse column '%s'\n", argv[3]);
    return EXIT_FAILURE;
  }

  if (mmap_file(argv[1], &ctx.file) == 0) {
    TSParser *parser       = ts_parser_new();
    const TSLanguage *lang = tree_sitter_c();
    ts_parser_set_language(parser, lang);

    {
      TSNode root;
      ctx.tree = ts_parser_parse_string(parser, NULL, ctx.file.content,
                                        (uint32_t)ctx.file.length);
      if (!ctx.tree) {
        fprintf(stderr, "failed to parse\n");
        res = EXIT_FAILURE;
        goto Lerr;
      }

      /* ts_tree_print_dot_graph(tree, stdout); */
      root = ts_tree_root_node(ctx.tree);
      if (!ts_node_is_null(root)) {
        TSNode highligted;
#if 0
        {
          char *p = ts_node_string(root);
          fprintf(stdout, "%s\n", p);
          free(p);
        }
#endif

        highligted = ts_node_descendant_for_point_range(root, pos, pos);
        if (!ts_node_is_null(highligted)) {
          TSNode found = sp_find_struct_specifier(highligted, &ctx.file);
          if (!ts_node_is_null(found)) {
            sp_print_struct(&ctx, found);
          }
        } else {
          fprintf(stderr, "out of range %u,%u\n", pos.row, pos.column);
          res = EXIT_FAILURE;
        }
      } else {
        fprintf(stderr, "Tree is empty \n");
        res = EXIT_FAILURE;
      }
      ts_tree_delete(ctx.tree);
    }

  Lerr:
    ts_parser_delete(parser);
    /* munmap(file, flength); */
  } else {
    res = EXIT_FAILURE;
  }

  return res;
}
