import sys

text = open('cpp_frontend/main.cpp').read()
text = text.replace('std/image_backend.cpp', 'std/string_backend.cpp std/image_backend.cpp')
open('cpp_frontend/main.cpp', 'w').write(text)
