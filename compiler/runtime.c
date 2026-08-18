#include <stdio.h>
#include <stdlib.h>
#include <string.h>

extern "C" {
    void* alloc(int bytes) {
        return malloc(bytes);
    }
    void print(const char* msg) {
        printf("%s\n", msg ? msg : "null");
    }
    void print_str_3(const char* m1, const char* m2, const char* m3) {
        printf("%s%s%s\n", m1 ? m1 : "null", m2 ? m2 : "null", m3 ? m3 : "null");
    }
    int str_eq(const char* a, const char* b) {
        if (!a || !b) return 0;
        return strcmp(a, b) == 0 ? 1 : 0;
    }
    void* get_null_ast() { return NULL; }
    void* get_null_scope() { return NULL; }
    void* get_null_symbol() { return NULL; }
    void* get_null_type() { return NULL; }
}
