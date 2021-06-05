#include <tree_sitter/api.h>

#include "shared.h"

#include <string.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <stdint.h>
#include <errno.h>

extern const TSLanguage *
tree_sitter_c(void);

static bool
sp_parse_uint32_t(const char *in, uint32_t *out)
{
  char *end          = NULL;
  const char *in_end = in + strlen(in);
  long val;

  val = strtol(in, &end, 10);
  if (end != in_end) {
    return false;
  }

  if (errno == ERANGE) {
    return false;
  }

  if (val < 0) {
    return false;
  }
  if (val >= UINT32_MAX) {
    return false;
  }

  *out = (uint32_t)val;

  return true;
}

int
main(int argc, const char *argv[])
{
  int res = EXIT_SUCCESS;
  uint32_t line;
  uint32_t column;

  if (argc != 4) {
    fprintf(stderr, "%s file line column\n", argv[0]);
    return EXIT_FAILURE;
  }

  if (!sp_parse_uint32_t(argv[2], &line)) {
    fprintf(stderr, "failed to parse line '%s'\n", argv[2]);
    return EXIT_FAILURE;
  }
  if (!sp_parse_uint32_t(argv[3], &column)) {
    fprintf(stderr, "failed to parse column '%s'\n", argv[3]);
    return EXIT_FAILURE;
  }

  {
    char *file;
    size_t flength = 0;

    if (!(file = mmap_file(argv[1], &flength))) {
      return EXIT_FAILURE;
    }

    {
      TSParser *parser       = ts_parser_new();
      const TSLanguage *lang = tree_sitter_c();
      ts_parser_set_language(parser, lang);

      {
        TSNode root;
        TSTree *tree;

        tree = ts_parser_parse_string(parser, NULL, file, (uint32_t)flength);
        if (!tree) {
          fprintf(stderr, "failed to parse\n");
          res = EXIT_FAILURE;
          goto Lerr;
        }
        root = ts_tree_root_node(tree);
        /*
         * TSNode highligted =
         *   ts_node_descendant_for_point_range(root, TSPoint, TSPoint);
         * if (ts_node_is_null(highligted)) {
         *   fprintf(stderr, "out of range %u,%u\n", line, column);
         *   res = EXIT_FAILURE;
         *   goto Lerr;
         * }
         */
        ts_tree_delete(tree);
      }

    Lerr:
      ts_parser_delete(parser);
    }
    munmap(file, flength);
  }

  return res;
}
