#include <tree_sitter/api.h>

#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/mman.h>
#include <sys/stat.h>

/* API:
 * /home/spooky/sources/tree-sitter/lib/include/tree_sitter
 */

extern const TSLanguage *
tree_sitter_c(void);


int
main(int argc, char *argv[])
{
  TSTree *old_tree = NULL, *new_tree = NULL;
  /* TSTree *old_tree_copy  = NULL; */
  TSTree *new_tree_copy = NULL, *reverse_tree = NULL;
  TSParser *parser       = ts_parser_new();
  const TSLanguage *lang = tree_sitter_c();

  (void)argc;
  (void)argv;

  char initial_file[128]         = {0};
  char updated_file[128]         = {0};
  const char *original           = "int dummy;struct sec* second;";
  const char *update             = "struct argh *member;";
  const char *const file_pattern = "struct st {\n%s\n} instance;";
  sprintf(initial_file, file_pattern, original);
  sprintf(updated_file, file_pattern, update);

  ts_parser_set_language(parser, lang);
  /* inital tree */
  old_tree = ts_parser_parse_string(parser, NULL, initial_file,
                                    (uint32_t)strlen(initial_file));
  if (!old_tree) {
    return 1;
  }

  /* ts_tree_print_dot_graph(old_tree, stdout); */
  /* printf("%s\n", ts_node_string(ts_tree_root_node(old_tree))); */
  {
    /* make some change */
    uint32_t start_byte = 12;
    TSInputEdit edit    = {
      .start_byte   = start_byte,
      .old_end_byte = start_byte + (uint32_t)strlen(original),
      .new_end_byte = start_byte + (uint32_t)strlen(update),
      .start_point =
        {
          .row    = 1,
          .column = 0,
        },
      .old_end_point =
        {
          .row    = 1,
          .column = (uint32_t)strlen(original),
        },
      .new_end_point =
        {
          .row    = 1,
          .column = (uint32_t)strlen(update),
        },
    };
    TSInputEdit reverse_edit = {
      .start_byte    = edit.start_byte,
      .old_end_byte  = edit.new_end_byte,
      .new_end_byte  = edit.old_end_byte,
      .start_point   = edit.start_point,
      .old_end_point = edit.new_end_point,
      .new_end_point = edit.old_end_point,
    };
#if 0
typedef struct {
  uint32_t start_byte;
  uint32_t old_end_byte;
  uint32_t new_end_byte;
  TSPoint start_point;
  TSPoint old_end_point;
  TSPoint new_end_point;
} TSInputEdit;

• on_bytes:
 • start row of the changed text (zero-indexed)
 • start column of the changed text
 • byte offset of the changed text (from the start of the buffer)
 • old end row of the changed text
 • old end column of the changed text
 • old end byte length of the changed text
 • new end row of the changed text
 • new end column of the changed text
 • new end byte length of the changed text

def _on_bytes(bufnr, changed_tick, start_row, start_col, start_byte, old_row, old_col, old_byte, new_row, new_col, new_byte)
  local old_end_col = old_col + ((old_row == 0) and start_col or 0)
  local new_end_col = new_col + ((new_row == 0) and start_col or 0)
  tree.edit(start_byte,start_byte+old_byte,start_byte+new_byte,
    start_row, start_col,
    start_row+old_row, old_end_col,
    start_row+new_row, new_end_col)

#endif
    ts_tree_edit(old_tree, &edit); /* -> void */
    new_tree = ts_parser_parse_string(parser, old_tree, updated_file,
                                      (uint32_t)strlen(updated_file));

    new_tree_copy = ts_tree_copy(new_tree);
    ts_tree_edit(new_tree_copy, &reverse_edit);
    reverse_tree = ts_parser_parse_string(parser, new_tree_copy, initial_file,
                                          (uint32_t)strlen(initial_file));
  }
  printf("reverse_tree: is_error:%s\n",
         ts_node_has_error(ts_tree_root_node(reverse_tree)) ? "TRUE" : "FALSE");
  /* printf("%s\n", ts_node_string(ts_tree_root_node(reverse_tree))); */
  /* printf("%s\n", ts_node_string(ts_tree_root_node(new_tree))); */
  /*whats added*/
  if (old_tree && new_tree) {
    size_t i;
    TSRange *changed; //array of $n_changed nodes
    uint32_t n_changed;
    // TODO:
    // - we want to know about the new nodes
    //   - run the query on range: to know what is removed
    // - we also want to know about the delted nodes
    //  - run the query on range: to know what is no longer there
    changed = ts_tree_get_changed_ranges(old_tree, new_tree, &n_changed);
    printf("added n_changed:%u\n", n_changed);
    for (i = 0; i < n_changed; ++i) {
      TSNode n;
      n = ts_node_descendant_for_byte_range(ts_tree_root_node(new_tree),
                                            changed[i].start_byte,
                                            changed[i].end_byte);

      if (!ts_node_is_null(n)) {
#if 1
        uint32_t s           = ts_node_start_byte(n);
        uint32_t e           = ts_node_end_byte(n);
        uint32_t span_length = e - s;
        /* printf("%s\n", ts_node_string(n)); */
        printf("%.*s: %s\n", (int)span_length, &updated_file[s],
               ts_node_type(n));
#endif
#if 0
        TSTreeCursor c = ts_tree_cursor_new(n);
        if (ts_tree_cursor_goto_first_child(&c)) {
          do {
            TSNode child         = ts_tree_cursor_current_node(&c);
            uint32_t s           = ts_node_start_byte(child);
            uint32_t e           = ts_node_end_byte(child);
            uint32_t span_length = e - s;
            printf("%.*s\n", (int)span_length, &updated_file[s]);
          } while (ts_tree_cursor_goto_next_sibling(&c));
        }
#endif
      }
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

    } //for
    free(changed);
  } //if

  /* whats removed */
  if (new_tree_copy && reverse_tree) {
    size_t i;
    TSRange *changed;
    uint32_t n_changed;
    changed =
      ts_tree_get_changed_ranges(new_tree_copy, reverse_tree, &n_changed);
    printf("removed n_changed:%u\n", n_changed);
    for (i = 0; i < n_changed; ++i) {
      TSNode n;
      n = ts_node_descendant_for_byte_range(ts_tree_root_node(new_tree),
                                            changed[i].start_byte,
                                            changed[i].end_byte);

      if (!ts_node_is_null(n)) {
        uint32_t s           = ts_node_start_byte(n);
        uint32_t e           = ts_node_end_byte(n);
        uint32_t span_length = e - s;
        /* printf("%s\n", ts_node_string(n)); */
        printf("%.*s:%s\n", (int)span_length, &initial_file[s],
               ts_node_type(n));
      }

    } //for
    free(changed);
  }
#if 0
  if (old_tree_copy && new_tree) {
    size_t i;
    TSRange *changed; //array of $n_changed nodes
    uint32_t n_changed;

    changed = ts_tree_get_changed_ranges(old_tree, new_tree, &n_changed);
    printf("removed n_changed:%u\n", n_changed);
    for (i = 0; i < n_changed; ++i) {
      TSNode n;
      n = ts_node_descendant_for_byte_range(ts_tree_root_node(old_tree_copy),
                                            changed[i].start_byte,
                                            changed[i].end_byte);

      if (!ts_node_is_null(n)) {
        uint32_t s           = ts_node_start_byte(n);
        uint32_t e           = ts_node_end_byte(n);
        uint32_t span_length = e - s;
        printf("%.*s: %s\n", (int)span_length, &initial_file[s], ts_node_type(n));
      }
    } //for
    free(changed);
  }
#endif

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
