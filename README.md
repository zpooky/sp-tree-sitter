#
## install
git submodule update --init
npm install -g tree-sitter-cli
poetry install

##
poetry run python .

##
git clone https://github.com/tree-sitter/py-tree-sitter.git
https://pypi.org/project/tree-sitter/
https://github.com/nvim-treesitter/nvim-treesitter


https://tree-sitter.github.io/tree-sitter/syntax-highlighting

## nvim treesitter example module using AST
https://github.com/p00f/nvim-ts-rainbow/blob/master/lua/rainbow/internal.lua
https://github.com/p00f/nvim-ts-rainbow/blob/adad3ea7eb820b7e0cc926438605e7637ee1f5e6/lua/rainbow/internal.lua " API"

##
https://github.com/tree-sitter/tree-sitter-c

##
```vim
syn keyword cGlobalVariable <Variable>
" Unfortunately, the granularity for removal of syntax items is limited to whole syntax groups (here: cGlobalVariable)
syn clear cGlobalVariable
```

au Syntax c call MyCadd()
function MyCadd()
  syn keyword cMyItem contained Ni
  syn cluster cCommentGroup add=cMyItem
  hi link cMyItem Title
endfun

-------------------------------------------------------------------------------
# TODO better tree refresh
```

./src/nvim/lua/treesitter.h
./src/nvim/lua/treesitter.c

./runtime/doc/treesitter.txt
./runtime/lua/vim/treesitter.lua

~/sources/neovim/runtime/lua/vim/treesitter:
.
├── highlighter.lua
├── language.lua
├── languagetree.lua
└── query.lua



/home/spooky/sources/neovim/runtime/lua/vim/treesitter/languagetree.lua
--- Registers callbacks for the parser
-- @param cbs An `nvim_buf_attach`-like table argument with the following keys :
--  `on_bytes` : see `nvim_buf_attach`, but this will be called _after_ the parsers callback.
--  `on_changedtree` : a callback that will be called every time the tree has syntactical changes.
--      it will only be passed one argument, that is a table of the ranges (as node ranges) that
--      changed.
--  `on_child_added` : emitted when a child is added to the tree.
--  `on_child_removed` : emitted when a child is removed from the tree.
function LanguageTree:register_cbs(cbs)

vim.treesitter.get_parser()
```


```vim
function! s:FoldableRegion(tag, name, expr)
  let synexpr = 'syntax region ' . a:name . ' ' . a:expr
  let pfx = 'g:lua_syntax_fold_'
  if !exists('g:lua_syntax_nofold') || exists(pfx . a:tag) || exists(pfx . a:name)
    let synexpr .= ' fold'
  end
  exec synexpr
endfunction

" Symbols
call s:FoldableRegion('table', 'luaTable',
      \ 'transparent matchgroup=luaBraces start="{" end="}" contains=@luaExpr')
```


-------------------------------------------------------------------------------
TODO local shadow of global variable
```c
TODO: __malloc gets highligted

extern __printf(3, 0)
char *devm_kvasprintf() __malloc;

(translation_unit 
	(declaration (storage_class_specifier)
	 type: (macro_type_specifier 
				  name: (identifier)
				 (ERROR (number_literal) 
								(number_literal)
							  (type_descriptor type: (primitive_type) declarator: (abstract_pointer_declarator))
				 )
				 type: (type_descriptor type: 
							 (type_identifier) 
							 declarator: (abstract_function_declarator parameters: (parameter_list))) (MISSING ")")
	) declarator: (identifier))
)
__malloc: global:True
```

TODO
```
static int global_static_init = 1;
extern char* global_extern_init = "wasd";
const float global_data_init = 1.f;
volatile float global_volatile_data_init = global_data_init;
```
