#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>

// Mock host allocator for a 2x3 byte array
uint8_t** test_host_alloc() {
    uint8_t** matrix = (uint8_t**)malloc(sizeof(uint8_t*) * 2);
    matrix[0] = (uint8_t*)malloc(sizeof(uint8_t) * 3);
    matrix[1] = (uint8_t*)malloc(sizeof(uint8_t) * 3);
    
    matrix[0][0] = 10;
    matrix[0][1] = 15;
    matrix[0][2] = 20;
    
    matrix[1][0] = 30;
    matrix[1][1] = 40;
    matrix[1][2] = 50; // We read matrix[1][2] -> 50
    
    return matrix;
}

int print_num(int n) {
    printf("%d\n", n);
    return 0;
}
