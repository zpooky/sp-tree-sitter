#include <tree-sitter/api.h>

#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>

/* API:
 * /home/spooky/sources/tree-sitter/lib/include/tree_sitter
 */

static char *
mmap_file(const char *file, size_t *bytes)
{
  char *addr;
  int fd;
  struct stat sb;
  off_t offset, pa_offset;
  ssize_t s;

  if ((fd = open(file, O_RDONLY) < 0)) {
    return NULL;
  }

  if (fstat(fd, &sb) < 0) {
    return NULL;
  }

  *bytes = (size_t)sb.st_size;
  addr   = mmap(NULL, *bytes, PROT_READ, MAP_PRIVATE, fd, 0);
  if (addr == MAP_FAILED)
    return NULL;

  return (char *)addr;
}

int
main()
{
  TSTree *old_tree = NULL;
  TSTree *new_tree = NULL;
  TSParser *parser = ts_parser_new();
  TSLanguage *lang = lang_parser(); // autgenerated?

  ts_parser_set_language(parser, lang);
  {
    /* inital tree */
    /* size_t bytes; */
    /* char *content = mmap_file("./test.c", &bytes); */
    const char *content = "struct st {\n"
                          "int dummy;\n"
                          "} instance;\n";
    bytes = strlen(content);

    old_tree = ts_parser_parse_string(parser, NULL, content, bytes);
  }

  /* ts_tree_print_dot_graph(old_tree, FILE *); */
  {
    /* make some change */
    const char *content = "struct st {\n"
                          "char dummy;\n"
                          "} instance;\n";
    bytes            = strlen(content);
    //TODO 
    // do we edit to old_tree? to later get the new_tree? then we corrupt old_tree?
    // I thought it was copy on write?
    // Is this correct?
    TSInputEdit edit = {};//TODO populate with all indeces
#if 0
typedef struct {
  uint32_t start_byte;
  uint32_t old_end_byte;
  uint32_t new_end_byte;
  TSPoint start_point;
  TSPoint old_end_point;
  TSPoint new_end_point;
} TSInputEdit;
#endif
    ts_tree_edit(old_tree, &edit); /* -> void */
    old_tree = ts_parser_parse_string(parser, old_tree, content, bytes);
    if (old_tree && new_tree) {
      size_t i;
      /*delta*/
      TSRange *changed; //array of $n_changed nodes
      size_t n_changed;
      // TODO:
      // - we want to know about the new nodes
      //   - run the query on range: to know what is removed
      // - we also want to know about the delted nodes
      //  - run the query on range: to know what is no longer there
      changed = ts_tree_get_changed_ranges(old_tree, new_tree, &n_changed);
      for(i=0;i<n_changed;++i){


#if 0
typedef struct {
  uint32_t row;
  uint32_t column;
} TSPoint;

typedef struct {
  TSPoint start_point;
  TSPoint end_point;
  uint32_t start_byte;
  uint32_t end_byte;
} TSRange;
void ts_query_cursor_set_byte_range(TSQueryCursor *, uint32_t, uint32_t);
#endif



      }//for
    }
  }
  ts_parser_delete(parser);
  return 0;
}

/* for change in changes:
 *   parser_set_range(change)
 * https://github.com/neovim/neovim/blob/2d36b62eda16ae1cf370f4107530fa65d2b1bce4/src/nvim/lua/treesitter.c#L503
 *   parser_parser():
 * https://github.com/neovim/neovim/blob/2d36b62eda16ae1cf370f4107530fa65d2b1bce4/src/nvim/lua/treesitter.c#L234
 */

/*
 * partion file into zone containing different languages
 * ts_parser_set_included_ranges(*p, ranges, tbl_len);
 */
