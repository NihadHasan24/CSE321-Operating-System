#include <stdio.h>
#include <stdlib.h>

int main(int argc, char *argv[]) {
    if (argc < 2) {
        return 1;
    }
    
    int n = argc - 1;
    
    printf("Odd/Even status:\n");
    for (int i = 1; i < argc; i++) {
        int num = atoi(argv[i]);
        if (num % 2 == 0) {
            printf("%d is Even\n", num);
        } else {
            printf("%d is Odd\n", num);
        }
    }
    
    return 0;
}
