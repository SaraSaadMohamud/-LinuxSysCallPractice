#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/wait.h>
#include <pwd.h>
#include <errno.h>

int picoshell_main(int argc, char *argv[]) 
{
    (void)argc;
    (void)argv;

    char input[1024];
    int state = 0;
    
    while (1) 
    {
        printf("Pico shell prompt > \n"); 
        fflush(stdout);

        if (fgets(input, sizeof(input), stdin) == NULL) 
        {
            return state;
        }
        size_t len = strlen(input);
        if (len > 0 && input[len - 1] == '\n') 
        {
            input[len - 1] = '\0';
        }

        if (input[0] == '\0') 
        {
            continue;
        }
        char *args[1000000];
        int arg_count = 0;
        
        char *token = strtok(input, " ");
        while (token != NULL && arg_count < 1000000) 
        {
            args[arg_count++] = token;
            token = strtok(NULL, " ");
        }
        args[arg_count] = NULL;

        if (arg_count == 0) 
        {
            continue;  
        }

        if (strcmp(args[0], "exit") == 0) 
        {
            printf("Good Bye\n");
            return state;
        }
        else if (strcmp(args[0], "echo") == 0)
        {
            for (int i = 1; i < arg_count; i++) 
            {
                printf("%s", args[i]);
                if (i < arg_count - 1) 
                {
                    printf(" ");
                }
            }
            printf("\n");
            state = 0;
        }
        else if (strcmp(args[0], "pwd") == 0)
        {
            char cwd[1024];
            if (getcwd(cwd, sizeof(cwd)) != NULL) 
            {
                printf("%s\n", cwd);
                state = 0;
            } 
            else 
            {
                perror("pwd failed");
                state = 1;
            }
        }
        else if (strcmp(args[0], "cd") == 0) 
        {
            char *target_dir = NULL;
            if (arg_count < 2) 
            {
                target_dir = getenv("HOME");
                if (!target_dir) 
                {
                    struct passwd *pw = getpwuid(getuid());
                    if (pw) target_dir = pw->pw_dir;
                }
                if (!target_dir) 
                {
                    fprintf(stderr, "cd: cannot determine home directory\n");
                    state = 1;
                    continue;
                }
            } 
            else 
            {
                target_dir = args[1];
            }

            if (chdir(target_dir) != 0) 
            {
                fprintf(stderr, "cd: %s: %s\n", target_dir, strerror(errno));
                state = 1;
            } 
            else 
            {
                state = 0;
            }
        }
        else 
        {
            pid_t pid = fork();
            if (pid < 0) {
                perror("fork failed");
                state = 1;
            }
            else if (pid == 0) {
                execvp(args[0], args);
                fprintf(stderr, "%s: command not found\n", args[0]);
                exit(1);
            }
            else 
            {
                int status;
                wait(&status);
                if (WIFEXITED(status)) {
                    state = WEXITSTATUS(status);
                }
            }
        }
    }

    return state;
}
