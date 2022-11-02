#include <stdio.h>

int f(int t) {
    return t+1;
}

int main() {
    int fval = f(4);
    printf("f(): %d\n", fval);
    return 0;
}