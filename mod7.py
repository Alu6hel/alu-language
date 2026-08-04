import sys
content = open('std/png.alu').read()
content = content.replace('int mask = (1 << count) - 1;', 'printf("byte_pos: %d, bit_pos: %d, count: %d, bit_buf: %d\\n", bs.byte_pos, bs.bit_count, count, bs.bit_buf); int mask = (1 << count) - 1;')
open('std/png.alu', 'w').write(content)
