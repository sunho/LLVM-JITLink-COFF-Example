#include <stdio.h>
#include <map>

int main() {
    std::map<int, int> M;
    M[1] = 53;
    printf("M[1] = %d\n", M[1]);
    return 0;
}