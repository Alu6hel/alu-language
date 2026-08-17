import re

def fix(filename):
    with open(filename, "r") as f:
        text = f.read()
    
    text = re.sub(r"let\s+([a-zA-Z0-9_]+)\s*:\s*([a-zA-Z0-9_<>]+)\s*=", r"\2 \1 =", text)
    text = text.replace("Vector_new", "vec_new")
    text = text.replace("Vector_push", "vec_push")
    text = text.replace("Vector_size", "vec_len")
    text = text.replace("Vector_get", "vec_get")
    
    with open(filename, "w") as f:
        f.write(text)

fix("std/string.alu")
fix("tests/test_string.alu")
