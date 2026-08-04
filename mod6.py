import sys
content = open('std/png.alu').read()
content = content.replace('int result = 0;', 'int result = 0; if (this.byte_pos == 0 && this.bit_pos == 0) { printf("BitStream data[0]: %d\\n", int(this.data[0]) & 255); }')
open('std/png.alu', 'w').write(content)
