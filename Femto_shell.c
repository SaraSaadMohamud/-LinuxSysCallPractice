#include <stdio.h>
#include <string.h>

int femtoshell_main(int argc, char *argv[]) 
{
    char input[1000000];
    int state = 0;
    while (1) 
    {
        printf("Femto shell prompt > ");
        if (fgets(input, sizeof(input), stdin) == NULL) 
            break; 

        size_t len = strlen(input);
        if (len > 0 && input[len - 1] == '\n') 
            input[len - 1] = '\0'; 

        char *cmd = strtok(input, " ");
        if (cmd == NULL) 
            continue; 

        if (strcmp(cmd, "exit") == 0) 
        {
            printf("Good Bye\n");
            return state; 
        }
        else if (strcmp(cmd, "echo") == 0) 
        {
            char *msg = strtok(NULL, "\0");
            if (msg != NULL) 
            {
                printf("%s\n", msg);
                state = 0;
            }
            else 
            {
                printf("\n");
                state = 0;
            }
        }
        else 
        {
            printf("Invalid command\n");
            state = 1; 
        }
    }
    return state; 
}
