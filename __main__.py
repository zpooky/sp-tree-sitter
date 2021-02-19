#!/usr/bin/env python3
# -*- coding: utf-8 -*-

import os
from tree_sitter import Language, Parser

Language.build_library('build/languages.so',
                       ['lang/tree-sitter-c', 'lang/tree-sitter-python'])
C_LANGUAGE = Language('build/languages.so', 'c')
PY_LANGUAGE = Language('build/languages.so', 'python')

parser = Parser()
parser.set_language(C_LANGUAGE)
#
# # f = open("cluster.c", "rb")
# # source = f.read()
# # tree = parser.parse(source)
tmp = """
int main(){
  return 0;
}
"""
# tmp = """
# def foo():
#     if bar:
#         baz()
# """
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


def build_json_tree(node):
  d = {
      "type": node.type,
  }
  if node.type == "string" or len(node.children) == 0:
    d['string'] = get_span(node)
  if len(node.children) > 0:
    d["children"] = [build_json_tree(child) for child in node.children]
  return d


root_node = tree.root_node
# print(root_node.type)
# print(root_node)
import pprint
pprint.pprint(build_json_tree(root_node))
# assert root_node.type == 'module'
# assert root_node.start_point == (1, 0)
# assert root_node.end_point == (3, 13)

# TODO try out query
