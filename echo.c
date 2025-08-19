#include <stdio.h>
#include <stdlib.h>

int echo_main(int argc, char *argv[]) {
    if (argc < 2) {
        printf("\n");
        return 0;
    } else {
        for (int i = 1; i < argc; i++) {
            printf("%s", argv[i]);
            if (i < argc - 1) {
                printf(" ");
            }
        }
        printf("\n");
        return 0;
    }
}