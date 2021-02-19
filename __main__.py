#!/usr/bin/env python3
# -*- coding: utf-8 -*-

import os
# from tree_sitter import Language, Parser
import tree_sitter as ts

ts.Language.build_library('build/languages.so',
                       ['lang/tree-sitter-c', 'lang/tree-sitter-python'])
C_LANGUAGE = ts.Language('build/languages.so', 'c')
PY_LANGUAGE = ts.Language('build/languages.so', 'python')

parser = ts.Parser()
parser.set_language(C_LANGUAGE)

file = "test.c"
if 1 == 1:
  f = open(file, "rb")
  bytecode = f.read()
  tree = parser.parse(bytecode)
else:
  tmp = """
  int main() {
    return 0;
  }
  """
  bytecode = bytes(tmp, "utf8")
  tree = parser.parse(bytecode)

#
# cursor = tree.walk()
# print(cursor.current_field_name())
# cursor.goto_parent()
# print(cursor.current_field_name())
# cursor.goto_first_child()
# print(cursor.current_field_name())
# # cursor.goto_parent # ->bool
# # cursor.goto_first_child # ->bool
# # cursor.goto_next_sibling # ->bool
# print(tree)
# print(cursor)
#
# tree_sitter.Query(C_LANGUAGE, source, len(source))

# parser = Parser()
# parser.set_language(PY_LANGUAGE)
#
# tree = parser.parse(bytes("""
# def foo():
# 	if bar:
# 		baz()
# """, "utf8"))


def get_span(node):
  return str(bytecode[node.start_byte:node.end_byte], "utf8")


def build_json(node):
  d = {
      "type": node.type,
  }
  if node.type == "string" or len(node.children) == 0:
    d['string'] = get_span(node)
  if len(node.children) > 0:
    d["children"] = [build_json(child) for child in node.children]
  return d


# print(tree.root_node.type)
# print(tree.root_node)
import pprint
pprint.pprint(build_json(tree.root_node))

query_text = """
(function_definition name: (identifier) @function.def)
"""
query = C_LANGUAGE.query(query_text)
captures = query.captures(tree.root_node)

