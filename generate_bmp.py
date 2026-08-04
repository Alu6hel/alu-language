import struct

def generate_bmp(filename, width, height):
    # BMP Header
    filesize = 54 + 3 * width * height
    header = struct.pack('<2sIHHI', b'BM', filesize, 0, 0, 54)
    
    # DIB Header
    dib_header = struct.pack('<IiiHHIIiiII', 40, width, height, 1, 24, 0, 3 * width * height, 2835, 2835, 0, 0)
    
    with open(filename, 'wb') as f:
        f.write(header)
        f.write(dib_header)
        
        # Pixels (BGR format, bottom-up)
        # Create a simple red-blue gradient
        for y in range(height):
            for x in range(width):
                b = int((x / width) * 255)
                g = 0
                r = int((y / height) * 255)
                f.write(struct.pack('<BBB', b, g, r))
            # Pad row to multiple of 4 bytes
            padding = (4 - ((width * 3) % 4)) % 4
            for _ in range(padding):
                f.write(b'\x00')

generate_bmp('test.bmp', 64, 64)
print("test.bmp generated successfully.")
