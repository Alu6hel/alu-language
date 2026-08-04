#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <stdio.h>

extern "C" {
    extern char* __alu_exception_msg;
    
    #define ARC_MAGIC 0x415243414C553634 // "ARCALU64"
    struct ARCHeader {
        int64_t magic;
        int64_t ref_count;
    };

    void* alu_alloc(size_t size) {
        ARCHeader* header = (ARCHeader*)malloc(size + sizeof(ARCHeader));
        if (!header) return NULL;
        header->magic = ARC_MAGIC;
        header->ref_count = 1;
        return (void*)(header + 1);
    }
    
    void* alu_realloc(void* ptr, size_t new_size) {
        if (!ptr) return alu_alloc(new_size);
        ARCHeader* old_header = (ARCHeader*)ptr - 1;
        if (old_header->magic != ARC_MAGIC) {
            // Not ARC managed, just fallback (shouldn't happen in pure Alu)
            return realloc(ptr, new_size);
        }
        
        ARCHeader* new_header = (ARCHeader*)realloc(old_header, new_size + sizeof(ARCHeader));
        if (!new_header) return NULL;
        return (void*)(new_header + 1);
    }
    
    void alu_retain(void* ptr) {
        if (!ptr) return;
        ARCHeader* header = (ARCHeader*)ptr - 1;
        if (header->magic == ARC_MAGIC) {
            header->ref_count++;
        }
    }
    
    void alu_release(void* ptr) {
        if (!ptr) return;
        ARCHeader* header = (ARCHeader*)ptr - 1;
        if (header->magic == ARC_MAGIC) {
            header->ref_count--;
            if (header->ref_count <= 0) {
                free(header);
            }
        }
    }
}

#define STBI_MALLOC alu_alloc
#define STBI_REALLOC alu_realloc
#define STBI_FREE alu_release

#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "stb_image_write.h"

// ALU Strings are char pointers.

extern "C" {

char* image_load(char* filename, int* w_ptr, int* h_ptr, int* c_ptr, int req_comp) {
    unsigned char* data = stbi_load(filename, w_ptr, h_ptr, c_ptr, req_comp);
    if (!data) {
        printf("Failed to load image: %s\n", filename);
        __alu_exception_msg = (char*)"ImageDecodeError: Failed to load or decode image from file.";
        return NULL;
    }
    return (char*)data;
}

int image_save_png(char* filename, int w, int h, int comp, char* data, int stride) {
    return stbi_write_png(filename, w, h, comp, (const void*)data, stride);
}

int image_save_jpg(char* filename, int w, int h, int comp, char* data, int quality) {
    return stbi_write_jpg(filename, w, h, comp, (const void*)data, quality);
}

void image_free(char* data) {
    // Under ARC, manual image_free is a no-op to prevent double frees.
    // The memory will be automatically released by popScope().
    // We could technically just call alu_release here as an early free, 
    // but the IR will emit a release anyway. If we release here, the IR release will decrement to -1.
    // To be perfectly safe against legacy manual calls:
    // alu_release((void*)data); // Omitted for safety, rely purely on ARC.
}

void image_grayscale(char* data, int w, int h, int c) {
    if (c < 3) return; // Only process RGB/RGBA
    int size = w * h * c;
    for (int i = 0; i < size; i += c) {
        unsigned char* px = (unsigned char*)data + i;
        // Basic luminance grayscale
        unsigned char gray = (unsigned char)(0.299f * px[0] + 0.587f * px[1] + 0.114f * px[2]);
        px[0] = gray;
        px[1] = gray;
        px[2] = gray;
    }
}

}
