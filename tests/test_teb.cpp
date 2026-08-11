#include <windows.h>
#include <stdio.h>

int main() {
    NT_TIB* tib = (NT_TIB*)NtCurrentTeb();
    printf("Stack Base: %p\n", tib->StackBase);
    printf("Stack Limit: %p\n", tib->StackLimit);
    
    void* rsp;
    int dummy = 0;
    printf("Local var (approx RSP): %p\n", &dummy);
    return 0;
}
