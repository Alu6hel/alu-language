#include <windows.h>
#include <stdio.h>

int main() {
    char stack_var[20] = "Hello";
    MEMORY_BASIC_INFORMATION mbi;
    printf("Calling VirtualQuery...\n");
    if (VirtualQuery((void*)stack_var, &mbi, sizeof(mbi)) == 0) {
        printf("Failed\n");
    } else {
        printf("Success\n");
    }
    return 0;
}
