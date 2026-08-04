import sys
content = open('std/png.alu').read()
content = content.replace('bs.bit_count = 0;', 'bs.bit_count = 0; bs.bit_buf = 0;')
open('std/png.alu', 'w').write(content)
