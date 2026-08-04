#include <stdint.h>
#include <string.h>

#define ROTL(a,b) (((a) << (b)) | ((a) >> (32 - (b))))
#define QR(a, b, c, d) ( \
    a += b, d ^= a, d = ROTL(d, 16), \
    c += d, b ^= c, b = ROTL(b, 12), \
    a += b, d ^= a, d = ROTL(d, 8), \
    c += d, b ^= c, b = ROTL(b, 7))

extern "C" {

void chacha20_block(uint32_t out[16], uint32_t const in[16]) {
    int i;
    for (i = 0; i < 16; ++i) out[i] = in[i];
    for (i = 0; i < 10; ++i) {
        QR(out[0], out[4], out[ 8], out[12]);
        QR(out[1], out[5], out[ 9], out[13]);
        QR(out[2], out[6], out[10], out[14]);
        QR(out[3], out[7], out[11], out[15]);
        QR(out[0], out[5], out[10], out[15]);
        QR(out[1], out[6], out[11], out[12]);
        QR(out[2], out[7], out[ 8], out[13]);
        QR(out[3], out[4], out[ 9], out[14]);
    }
    for (i = 0; i < 16; ++i) out[i] += in[i];
}

// Encrypt/Decrypt in place using ChaCha20. Hardcoded static key/nonce for Swarm P2P.
void alu_swarm_encrypt(char* data, int length) {
    uint32_t state[16] = {
        0x61707865, 0x3320646e, 0x79622d32, 0x6b206574,
        0x01020304, 0x05060708, 0x090a0b0c, 0x0d0e0f10, // Key
        0x11121314, 0x15161718, 0x191a1b1c, 0x1d1e1f20, // Key
        0x00000001, // Counter
        0x00000000, 0x00000000, 0x00000000 // Nonce
    };
    
    uint32_t block[16];
    int offset = 0;
    while (offset < length) {
        chacha20_block(block, state);
        state[12]++; // increment counter
        
        uint8_t* block_bytes = (uint8_t*)block;
        for (int i = 0; i < 64 && offset < length; ++i, ++offset) {
            data[offset] ^= block_bytes[i];
        }
    }
}

} // extern "C"
