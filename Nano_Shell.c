#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <limits.h>
#include <sys/wait.h>   

typedef struct ShellVar
{
    char *name;
    char *value;
    struct ShellVar *next;
} ShellVar;

static ShellVar *local_vars = NULL;

/* ---------- Function Prototypes ---------- */
static char* get_local_var(const char *name);
static void set_local_var(const char *name, const char *value);
static void export_var(const char *name);
static void substitute_vars(char **args, int *argc);

/* ---------- Helper Functions ---------- */
static char* get_local_var(const char *name) 
{
    ShellVar *cur = local_vars;
    while (cur) 
    {
        if (strcmp(cur->name, name) == 0)
        {
          return cur->value;
        }
        cur = cur->next;
    }
    return NULL;
}

static void set_local_var(const char *name, const char *value) 
{
    ShellVar *cur = local_vars;
    while (cur) 
    {
        if (strcmp(cur->name, name) == 0)
        {
            free(cur->value);
            cur->value = strdup(value);
            return;
        }
        cur = cur->next;
    }
    ShellVar *newVar = (ShellVar*) malloc(sizeof(ShellVar));
    newVar->name = strdup(name);
    newVar->value = strdup(value);
    newVar->next = local_vars;
    local_vars = newVar;
}

static void export_var(const char *name)
{
    char *val = get_local_var(name);
    if (val) 
    {
        setenv(name, val, 1);
    }
}

static void substitute_vars(char **args, int *argc) 
{
    for (int i = 0; i < *argc; i++) 
    {
        char *dollar = strchr(args[i], '$');
        if (!dollar) continue;

        char buffer[1024];
        buffer[0] = '\0';

        char *src = args[i];
        while ((dollar = strchr(src, '$')))
          {
            strncat(buffer, src, dollar - src);   

            char varname[128];
            int j = 0;
            dollar++;  
            while (*dollar && (isalnum(*dollar) || *dollar == '_')) 
            {
                varname[j++] = *dollar++;
            }
            varname[j] = '\0';

            char *val = get_local_var(varname);
            if (val) strcat(buffer, val); 
            src = dollar; 
        }
        strcat(buffer, src);  

        args[i] = strdup(buffer);
    }
}


/* ---------- Core Shell ---------- */
int nanoshell_main(int argc, char *argv[])
{
    (void)argc; (void)argv;
    int last_status = 0;
    char line[1024];
  
    while (fgets(line, sizeof(line), stdin))
      {
        size_t len = strlen(line);
        if (len > 0 && line[len-1] == '\n') line[len-1] = '\0';

        
        char *tokens[64];
        int ntok = 0;
        char *tok = strtok(line, " ");
        while (tok && ntok < 63) 
        {
            tokens[ntok++] = tok;
            tok = strtok(NULL, " ");
        }
        tokens[ntok] = NULL;

        if (ntok == 0) 
        {
            fflush(stdout);
            continue;
        }
        if (strchr(tokens[0], '=')) 
        {
            char *eq = strchr(tokens[0], '=');
            if (eq == tokens[0] || eq[1] == '\0' || ntok > 1) 
            {
                printf("Invalid command\n");
                last_status = 1;
            } 
            else 
            {
                *eq = '\0';
                set_local_var(tokens[0], eq+1);
                last_status = 0;
            }
            continue;
        }
        substitute_vars(tokens, &ntok);
        if (strcmp(tokens[0], "exit") == 0) 
        {
            printf("Good Bye\n");
            return last_status;
        }
        if (strcmp(tokens[0], "export") == 0 && ntok == 2) 
        {
            export_var(tokens[1]);
            last_status = 0;
            continue;
        }
        if (strcmp(tokens[0], "cd") == 0) 
        {
            const char *target = (ntok == 1) ? getenv("HOME") : tokens[1];
            if (!target) target = "/";
            if (chdir(target) != 0)
            {
                fprintf(stderr, "cd: %s: No such file or directory\n", target);
                last_status = 1;
            }
            else
            {
                last_status = 0;
            }
            continue;
        }
        if (strcmp(tokens[0], "pwd") == 0) 
        {
            char cwd[PATH_MAX];
            if (getcwd(cwd, sizeof(cwd)) != NULL) {
                printf("%s\n", cwd);
                last_status = 0;
            } else {
                perror("pwd");
                last_status = 1;
            }
            continue;
        }
        substitute_vars(tokens, &ntok);
        pid_t pid = fork();
        if (pid == 0)
        {
            execvp(tokens[0], tokens);
            fprintf(stderr, "%s: command not found\n", tokens[0]);
            exit(1);
        } 
        else if (pid > 0) 
        {
            int wstatus;
            wait(&wstatus);
            if (WIFEXITED(wstatus))
                last_status = WEXITSTATUS(wstatus);
        } 
        else
        {
            perror("fork");
            last_status = 1;
        }
    }
    return last_status;
}
