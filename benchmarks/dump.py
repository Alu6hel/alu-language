import sys
import re

def dump_tokens(filename):
    with open(filename, 'r') as f:
        content = f.read()
    
    # Just a simple hack to see the chars and tokens if we wrote the lexer in python, but we can't.
    # Instead, let's modify alu_cxx.cpp to print tokens!
