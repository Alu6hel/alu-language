import sys
content = open('std/png.alu').read()
content = content.replace('int idat_len = 0;', 'int idat_len = 0; printf("First bytes: %d %d\\n", int(file_data[41]) & 255, int(file_data[42]) & 255);')
open('std/png.alu', 'w').write(content)
