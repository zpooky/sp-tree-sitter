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
TODO local shadow of global variable
```
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
