import sys
content = open('std/png.alu').read()

crc_func = """
routine crc32(string data, int offset, int length) -> int {
    string crc_table = malloc(256 * 4);
    int i = 0;
    while (i < 256) {
        int c = i;
        int j = 0;
        while (j < 8) {
            if ((c & 1) != 0) {
                c = (0 - 306674912) ^ lsr(c, 1);
            } else {
                c = lsr(c, 1);
            }
            j = j + 1;
        }
        
        crc_table[i * 4] = c & 255;
        crc_table[i * 4 + 1] = lsr(c, 8) & 255;
        crc_table[i * 4 + 2] = lsr(c, 16) & 255;
        crc_table[i * 4 + 3] = lsr(c, 24) & 255;
        
        i = i + 1;
    }
    
    int crc = 0 - 1; // 0xFFFFFFFF
    i = 0;
    while (i < length) {
        int b = int(data[offset + i]) & 255;
        int index = (crc ^ b) & 255;
        
        int tab_val = (int(crc_table[index * 4]) & 255) |
                      ((int(crc_table[index * 4 + 1]) & 255) << 8) |
                      ((int(crc_table[index * 4 + 2]) & 255) << 16) |
                      ((int(crc_table[index * 4 + 3]) & 255) << 24);
                      
        crc = tab_val ^ lsr(crc, 8);
        i = i + 1;
    }
    
    free(crc_table);
    return crc ^ (0 - 1);
}
"""

# if crc32 already exists, replace it, else prepend
if 'routine crc32' in content:
    pass # we handle it by not prepending twice
else:
    content = crc_func + "\n" + content

# insert validation
if 'int computed_crc =' not in content:
    content = content.replace(
        'pos = pos + length;',
        '''
        int computed_crc = crc32(file_data, pos - 4, length + 4);
        pos = pos + length;
        int expected_crc = bytes_to_int(file_data, pos);
        if (computed_crc != expected_crc) {
            puts("CRC32 mismatch in chunk!");
        }
        '''
    )

open('std/png.alu', 'w').write(content)
