#include <stdio.h>
#include <stdlib.h>
#include <string.h>

extern "C" {
    // In our simplified prototype, we pass strings around. 
    // We'll cast the string to a FILE* inside the C wrapper.
    
    // Returns a pointer to the FILE stream, cast as a C-string for Alu compatibility
    char* fopen_c(const char* filename, const char* mode) {
        FILE* fp = fopen(filename, mode);
        return (char*)fp;
    }

    int fclose_c(char* stream) {
        if (!stream) return -1;
        return fclose((FILE*)stream);
    }

    int fread_c(char* ptr, int size, int nmemb, char* stream) {
        if (!stream) return -1;
        return fread(ptr, size, nmemb, (FILE*)stream);
    }

    int fwrite_c(const char* ptr, int size, int nmemb, char* stream) {
        if (!stream) return -1;
        return fwrite(ptr, size, nmemb, (FILE*)stream);
    }

    int fseek_c(char* stream, int offset, int whence) {
        if (!stream) return -1;
        return fseek((FILE*)stream, offset, whence);
    }

    int ftell_c(char* stream) {
        if (!stream) return -1;
        return ftell((FILE*)stream);
    }

    int feof_c(char* stream) {
        if (!stream) return -1;
        return feof((FILE*)stream);
    }
    
    int remove_c(const char* filename) {
        return remove(filename);
    }
}
