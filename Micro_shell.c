
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <fcntl.h>
#include <errno.h>

#define MAX_LINE 4096
#define MAX_ARGS 520

static char* skip_ws(char *p) {
    while (*p == ' ' || *p == '\t' || *p == '\n' || *p == '\r') ++p;
    return p;
}

static char* read_token(char **pp) {
    char *p = skip_ws(*pp);
    if (*p == '\0') { *pp = p; return NULL; }
    char *start = p;
    while (*p && *p!=' ' && *p!='\t' && *p!='\n' && *p!='\r') {
        if (*p=='<' || *p=='>') break;
        ++p;
    }
    if (p == start) { *pp = p; return NULL; }
    if (*p) { *p = '\0'; ++p; }
    *pp = p;
    return start;
}

static char* read_redir_target(char **pp) {
    char *p = *pp;
    p = skip_ws(p);
    if (*p == '\0') { *pp = p; return NULL; }
    if (*p == '<' || *p == '>') { *pp = p; return NULL; }
    char *start = p;
    while (*p && *p!=' ' && *p!='\t' && *p!='\n' && *p!='\r') {
        if (*p=='<' || *p=='>') break;
        ++p;
    }
    if (*p) { *p = '\0'; ++p; }
    *pp = p;
    return start;
}

int microshell_main(int argc, char *argv[]) {
    (void)argc; (void)argv;

    char line[MAX_LINE];
    int last_status = 0;

    while (1) {
        fputs("micro$ ", stdout);
        fflush(stdout);

        if (!fgets(line, sizeof(line), stdin))
        {
            if (feof(stdin)) 
            {
                break;
            }
            perror("fgets");
            break;
        }

        size_t n = strlen(line);
        if (n && line[n-1] == '\n') line[n-1] = '\0';

        char *p = skip_ws(line);
        if (*p == '\0') continue;

        if (strcmp(p, "exit") == 0) 
        {
            return(last_status);
        }
        char *argv_exec[MAX_ARGS];
        int argc_exec = 0;
        char *in_file = NULL, *out_file = NULL, *err_file = NULL;

        while (*p) {
            p = skip_ws(p);
            if (*p == '\0') break;

            if (p[0] == '2' && p[1] == '>') {
                p += 2;
                char *t = read_redir_target(&p);
                if (!t) { argc_exec = 0; break; }
                err_file = t;
                last_status =0;
                continue;
            }

            if (*p == '>') {
                p += 1;
                char *t = read_redir_target(&p);
                if (!t) { argc_exec = 0; break; }
                out_file = t;
                 last_status =0;
                continue;
            }

            if (*p == '<') {
                p += 1;
                char *t = read_redir_target(&p);
                if (!t) { argc_exec = 0; break; }
                in_file = t;
                 last_status =0;
                continue;
            }

            char *tok = read_token(&p);
            if (tok && argc_exec < MAX_ARGS - 1) {
                argv_exec[argc_exec++] = tok;
            } else if (!tok) {
            } else {
                argc_exec = 0;
                break;
            }
        }

        if (argc_exec == 0) continue;
        argv_exec[argc_exec] = NULL;

        int fd_in = -1, fd_out = -1, fd_err = -1;
        int open_failed = 0;

        if (in_file) {
            fd_in = open(in_file, O_RDONLY);
            if (fd_in < 0) {
                fprintf(stderr, "cannot access %s: %s\n", in_file, strerror(errno));
                open_failed = 1;
            }
        }
        if (!open_failed && out_file) {
            fd_out = open(out_file, O_WRONLY | O_CREAT | O_TRUNC, 0666);
            if (fd_out < 0) {
                fprintf(stderr, "%s: %s\n", out_file, strerror(errno));
                open_failed = 1;
            }
        }
        if (!open_failed && err_file) {
            fd_err = open(err_file, O_WRONLY | O_CREAT | O_TRUNC, 0666);
            if (fd_err < 0) {
                fprintf(stderr, "%s: %s\n", err_file, strerror(errno));
                open_failed = 1;
            }
        }

        if (open_failed) {
            if (fd_in >= 0) close(fd_in);
            if (fd_out >= 0) close(fd_out);
            if (fd_err >= 0) close(fd_err);
            last_status = 1;
            continue;
        }

        pid_t pid = fork();
        if (pid < 0) {
            perror("fork");
            if (fd_in >= 0) close(fd_in);
            if (fd_out >= 0) close(fd_out);
            if (fd_err >= 0) close(fd_err);
            last_status = 1;
            continue;
        }

        if (pid == 0) {
            if (fd_in >= 0) { dup2(fd_in, STDIN_FILENO); close(fd_in); }
            if (fd_out >= 0) { dup2(fd_out, STDOUT_FILENO); close(fd_out); }
            if (fd_err >= 0) { dup2(fd_err, STDERR_FILENO); close(fd_err); }
            execvp(argv_exec[0], argv_exec);
            exit(-1);
        } else {
            if (fd_in >= 0) close(fd_in);
            if (fd_out >= 0) close(fd_out);
            if (fd_err >= 0) close(fd_err);

            int status;
            if (waitpid(pid, &status, 0) > 0) {
                if (WIFEXITED(status)) last_status = WEXITSTATUS(status);
                else last_status = 1;
            }
        }
    }

    return last_status;
}
