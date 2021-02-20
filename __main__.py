#!/usr/bin/env python3
# -*- coding: utf-8 -*-

# Cursor:
# tree_cursor_methods[] = {
#     .ml_name = "current_field_name",
#     .ml_meth = (PyCFunction)tree_cursor_current_field_name,
#     .ml_flags = METH_NOARGS,
#     .ml_doc = "current_field_name()\n--\n\n\
#                Get the field name of the tree cursor's current node.\n\n\
#                If the current node has the field name, return str. Otherwise, return None.",
#
#     .ml_name = "goto_parent",
#     .ml_meth = (PyCFunction)tree_cursor_goto_parent,
#     .ml_flags = METH_NOARGS,
#     .ml_doc = "goto_parent()\n--\n\n\
#                Go to parent.\n\n\
#                If the current node is not the root, move to its parent and\n\
#                return True. Otherwise, return False.",
#
#     .ml_name = "goto_first_child",
#     .ml_meth = (PyCFunction)tree_cursor_goto_first_child,
#     .ml_flags = METH_NOARGS,
#     .ml_doc = "goto_first_child()\n--\n\n\
#                Go to first child.\n\n\
#                If the current node has children, move to the first child and\n\
#                return True. Otherwise, return False.",
#
#     .ml_name = "goto_next_sibling",
#     .ml_meth = (PyCFunction)tree_cursor_goto_next_sibling,
#     .ml_flags = METH_NOARGS,
#     .ml_doc = "goto_next_sibling()\n--\n\n\
#                Go to next sibling.\n\n\
#                If the current node has a next sibling, move to the next sibling\n\
#                and return True. Otherwise, return False.",
# };
# {"node", (getter)tree_cursor_get_node, NULL, "The current node.", NULL},

# Node:
# static PyMethodDef node_methods[] = {
#     .ml_name = "walk",
#     .ml_meth = (PyCFunction)node_walk,
#     .ml_flags = METH_NOARGS,
#     .ml_doc = "walk()\n--\n\n\
#                Get a tree cursor for walking the tree starting at this node.",
#
#     .ml_name = "sexp",
#     .ml_meth = (PyCFunction)node_sexp,
#     .ml_flags = METH_NOARGS,
#     .ml_doc = "sexp()\n--\n\n\
#                Get an S-expression representing the node.",
#
#     .ml_name = "child_by_field_id",
#     .ml_meth = (PyCFunction)node_chield_by_field_id,
#     .ml_flags = METH_VARARGS,
#     .ml_doc = "child_by_field_id(id)\n--\n\n\
#                Get child for the given field id.",
#
#     .ml_name = "child_by_field_name",
#     .ml_meth = (PyCFunction)node_chield_by_field_name,
#     .ml_flags = METH_VARARGS,
#     .ml_doc = "child_by_field_name(name)\n--\n\n\
#                Get child for the given field name.",
# };
#
# static PyGetSetDef node_accessors[] = {
#   {"type", (getter)node_get_type, NULL, "The node's type", NULL},
#   {"is_named", (getter)node_get_is_named, NULL, "Is this a named node", NULL},
#   {"is_missing", (getter)node_get_is_missing, NULL, "Is this a node inserted by the parser", NULL},
#   {"has_changes", (getter)node_get_has_changes, NULL, "Does this node have text changes since it was parsed", NULL},
#   {"has_error", (getter)node_get_has_error, NULL, "Does this node contain any errors", NULL},
#   {"start_byte", (getter)node_get_start_byte, NULL, "The node's start byte", NULL},
#   {"end_byte", (getter)node_get_end_byte, NULL, "The node's end byte", NULL},
#   {"start_point", (getter)node_get_start_point, NULL, "The node's start point", NULL},
#   {"end_point", (getter)node_get_end_point, NULL, "The node's end point", NULL},
#   {"children", (getter)node_get_children, NULL, "The node's children", NULL},
#   {"child_count", (getter)node_get_child_count, NULL, "The number of children for a node", NULL},
#   {"named_child_count", (getter)node_get_named_child_count, NULL, "The number of named children for a node", NULL},
#   {"next_sibling", (getter)node_get_next_sibling, NULL, "The node's next sibling", NULL},
#   {"prev_sibling", (getter)node_get_prev_sibling, NULL, "The node's previous sibling", NULL},
#   {"next_named_sibling", (getter)node_get_next_named_sibling, NULL, "The node's next named sibling", NULL},
#   {"prev_named_sibling", (getter)node_get_prev_named_sibling, NULL, "The node's previous named sibling", NULL},
#   {"parent", (getter)node_get_parent, NULL, "The node's parent", NULL},
#   };

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
# # cursor.goto_parent() # ->bool
# # cursor.goto_first_child() # ->bool
# # cursor.goto_next_sibling() # ->bool

# parser.set_language(PY_LANGUAGE)
# tree = parser.parse(bytes("""
# def foo():
# 	if bar:
# 		baz()
# """, "utf8"))


def get_span(node):
  return str(bytecode[node.start_byte:node.end_byte], "utf8")


def is_global(node):
  res = True
  # cursor = node.walk()
  # if not cursor is None:
  #   print(cursor.node)
  #   # cursor.goto_parent()
  #   cursor.goto_first_child()
  #   print(cursor.node)
  # print(node)
  while node.parent is not None:
    # print(node.type)
    if node.type == "function_definition":
      res = False
      break
    node = node.parent
  return res


def build_json(node):
  d = {
      "type": node.type,
  }
  if node.type == "string" or len(node.children) == 0:
    d['string'] = get_span(node)
  if len(node.children) > 0:
    d["children"] = [build_json(child) for child in node.children]
  return d


print(tree.root_node.sexp())

# print(tree.root_node.type)
# print(tree.root_node)

# import pprint
# pprint.pprint(build_json(tree.root_node))

# NOTE: query syntax seams to be language specific
query_text = """
(function_definition name: (identifier) @function.def)
"""
query_text = """
(declaration (identifier) @id)
"""
query = C_LANGUAGE.query(query_text)
captures = query.captures(tree.root_node)
# print(captures)
for i, (node, name) in enumerate(captures):
  # node_start = ','.join(map(str, node.start_point))
  # node_end = ','.join(map(str, node.end_point))
  print("{}: global:{}".format(get_span(node), is_global(node)))

# https://tree-sitter.github.io/tree-sitter/using-parsers#pattern-matching-with-queries
#  <type           >  <child-type  >   <child-type  >
# (binary_expression (number_literal) (number_literal))
