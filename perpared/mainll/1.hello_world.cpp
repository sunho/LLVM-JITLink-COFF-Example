#include <stdio.h>

int main() {
    printf("Hello, world!\n");
    return 0;
}

// clang -S -emit-llvm main.cpp