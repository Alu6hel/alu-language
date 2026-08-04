import sys
content = open('std/png.alu').read()
content = content.replace('int cmf = bs.read_bits(8);', 'int cmf = bs.read_bits(8); printf("CMF: %d\\n", cmf);')
content = content.replace('int flg = bs.read_bits(8);', 'int flg = bs.read_bits(8); printf("FLG: %d\\n", flg);')
content = content.replace('int btype = bs.read_bits(2);', 'int btype = bs.read_bits(2); printf("BTYPE: %d, BFINAL: %d\\n", btype, bfinal);')
open('std/png.alu', 'w').write(content)
