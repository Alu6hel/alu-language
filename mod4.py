import sys
content = open('std/png.alu').read()
content = content.replace('int file_len = read_file_legacy(filename, file_data);', 'int file_len = read_file_legacy(filename, file_data); printf("File[0]: %d, File[41]: %d\\n", int(file_data[0]) & 255, int(file_data[41]) & 255);')
open('std/png.alu', 'w').write(content)
