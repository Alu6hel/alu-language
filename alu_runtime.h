#ifndef ALU_RUNTIME_H
#define ALU_RUNTIME_H

#include <stdint.h>
#include <stdlib.h>

extern "C" {
    // The native Alu Automatic Reference Counting allocator
    void* alu_alloc(size_t size) {
        return malloc(size);
    }
    
    void* __alu_alloc_internal(int32_t size) {
        return malloc(size);
    }
    
    void alu_free(void* ptr) {
        free(ptr);
    }

    // Standard string representation in Alu memory
    struct AluString {
        int64_t ref_count; // ARC Reference Count injected by LLVM
        int64_t length;
        char* data;
    };
}

#endif
