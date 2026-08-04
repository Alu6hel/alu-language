import sys
content = open('std/png.alu').read()
content = content.replace('int out_cap =', 'printf("IDAT len: %d, IDAT[0]: %d, IDAT[1]: %d\\n", idat_len, int(idat_buf[0]) & 255, int(idat_buf[1]) & 255); int out_cap =')
open('std/png.alu', 'w').write(content)
