#include <stdio.h>
#include <map>

void throwException() {
    throw "ThrowExcpetion";
}

int main() {
    try {
        throwException();
    } catch(const char* M) {
        printf("Caught: %s\n", M);
    }
    return 0;
}