import sys
content = open('std/png.alu').read()
content = content.replace('if (file_len <= 8) {', 'printf("File size: %d\\n", file_len); if (file_len <= 8) {')
content = content.replace('int out_cap =', 'printf("IDAT len: %d, width: %d, height: %d, bpp: %d\\n", idat_len, width, height, bpp); int out_cap =')
open('std/png.alu', 'w').write(content)
