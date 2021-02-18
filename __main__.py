#!/usr/bin/env python3
# -*- coding: utf-8 -*-

import os
from tree_sitter import Language, Parser

Language.build_library('build/languages.so', [ 'lang/tree-sitter-c' ])
C_LANGUAGE = Language('build/languages.so', 'c')

parser = Parser()
parser.set_language(C_LANGUAGE)

parser.
