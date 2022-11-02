#include <stdio.h>
#include <Windows.h>
#include <map>

int reciprocal(int val) {
    return 1/val;
}

int main() {
    __try {
        reciprocal(0);
    } __except(EXCEPTION_EXECUTE_HANDLER) {
        printf("Div by zero!\n");
    }
    return 0;
}