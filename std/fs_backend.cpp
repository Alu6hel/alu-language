#include <stdio.h>
#include <stdlib.h>
#include <string.h>

extern "C" {
    void* alu_alloc(size_t size);

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

    char* fs_read_to_string_c(const char* path) {
        FILE* fp = fopen(path, "rb");
        if (!fp) return nullptr; // Or return ""
        fseek(fp, 0, SEEK_END);
        long size = ftell(fp);
        fseek(fp, 0, SEEK_SET);
        char* buffer = (char*)alu_alloc(size + 1);
        if (buffer) {
            fread(buffer, 1, size, fp);
            buffer[size] = '\0';
        }
        fclose(fp);
        return buffer;
    }

    int fs_write_string_c(const char* path, const char* content) {
        FILE* fp = fopen(path, "w");
        if (!fp) return 0;
        int res = fputs(content, fp) >= 0 ? 1 : 0;
        fclose(fp);
        return res;
    }

    int fs_append_string_c(const char* path, const char* content) {
        FILE* fp = fopen(path, "a");
        if (!fp) return 0;
        int res = fputs(content, fp) >= 0 ? 1 : 0;
        fclose(fp);
        return res;
    }

    int fs_exists_c(const char* path) {
        FILE* fp = fopen(path, "r");
        if (fp) {
            fclose(fp);
            return 1;
        }
        return 0;
    }

    int fs_size_c(const char* path) {
        FILE* fp = fopen(path, "rb");
        if (!fp) return -1;
        fseek(fp, 0, SEEK_END);
        long size = ftell(fp);
        fclose(fp);
        return size;
    }
    
    int fputs_c(const char* str, char* stream) {
        if (!stream || !str) return -1;
        return fputs(str, (FILE*)stream);
    }
}
