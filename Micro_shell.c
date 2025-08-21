#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <fcntl.h>
#include <errno.h>
#include <ctype.h>

#define MAX_LINE 4096
#define MAX_ARGS 256

static char* skip_ws(char *p) {
    while (*p == ' ' || *p == '\t' || *p == '\n' || *p == '\r') ++p;
    return p;
}

static char* expand_var(const char *token) {
    static char buf[MAX_LINE];
    int pos = 0;
    for (int i = 0; token[i] != '\0' && pos < MAX_LINE-1; ) {
        if (token[i] == '$') {
            i++;
            char name[256];
            int j = 0;
            while ((isalnum((unsigned char)token[i]) || token[i]=='_') && j < 255) {
                name[j++] = token[i++];
            }
            name[j] = '\0';
            char *val = getenv(name);
            if (val) {
                int len = strlen(val);
                if (pos + len < MAX_LINE-1) {
                    memcpy(buf+pos, val, len);
                    pos += len;
                }
            }
        } else {
            buf[pos++] = token[i++];
        }
    }
    buf[pos] = '\0';
    return strdup(buf); // caller must free
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

        if (!fgets(line, sizeof(line), stdin)) {
            if (feof(stdin)) break;
            perror("fgets");
            break;
        }

        size_t n = strlen(line);
        if (n && line[n-1] == '\n') line[n-1] = '\0';

        char *p = skip_ws(line);
        if (*p == '\0') continue;

        if (strcmp(p, "exit") == 0) break;

        char *eq = strchr(p, '=');
        if (eq && (p < eq)) {
            int only_assignment = 1;
            for (char *chk = eq+1; *chk; ++chk) {
                if (isspace((unsigned char)*chk)) { only_assignment = 0; break; }
            }
            if (only_assignment) {
                *eq = '\0';
                char *name = p;
                char *value = eq+1;
                if (setenv(name, value, 1) < 0) {
                    perror("setenv");
                    last_status = 1;
                } else {
                    last_status = 0;
                }
                continue;
            }
        }

        char *argv_exec[MAX_ARGS];
        int argc_exec = 0;

        /* track allocated strings to free later */
        char *alloced_args[MAX_ARGS]; int alloced_count = 0;
        char *in_file_str = NULL, *out_file_str = NULL, *err_file_str = NULL;

        int fd_in = -1, fd_out = -1, fd_err = -1;
        int open_failed = 0;

        while (*p) {
            p = skip_ws(p);
            if (*p == '\0') break;

            if (p[0] == '2' && p[1] == '>') {
                p += 2;
                char *t = read_redir_target(&p);
                if (!t) { argc_exec = 0; break; }
                /* expand variable inside filename if present */
                char *expanded = (strchr(t, '$') ? expand_var(t) : strdup(t));
                if (!expanded) { expanded = strdup(t); }
                int tmpfd = open(expanded, O_WRONLY | O_CREAT | O_TRUNC, 0666);
                if (tmpfd < 0) {
                    fprintf(stderr, "%s: %s\n", expanded, strerror(errno));
                    free(expanded);
                    open_failed = 1;
                    break;
                } else {
                    if (fd_err >= 0) close(fd_err);
                    fd_err = tmpfd;
                    if (err_file_str) free(err_file_str);
                    err_file_str = expanded;
                }
                continue;
            }
            if (*p == '>') {
                p += 1;
                char *t = read_redir_target(&p);
                if (!t) { argc_exec = 0; break; }
                char *expanded = (strchr(t, '$') ? expand_var(t) : strdup(t));
                if (!expanded) { expanded = strdup(t); }
                int tmpfd = open(expanded, O_WRONLY | O_CREAT | O_TRUNC, 0666);
                if (tmpfd < 0) {
                    fprintf(stderr, "%s: %s\n", expanded, strerror(errno));
                    free(expanded);
                    open_failed = 1;
                    break;
                } else {
                    if (fd_out >= 0) close(fd_out);
                    fd_out = tmpfd;
                    if (out_file_str) free(out_file_str);
                    out_file_str = expanded;
                }
                continue;
            }

            if (*p == '<') {
                p += 1;
                char *t = read_redir_target(&p);
                if (!t) { argc_exec = 0; break; }
                char *expanded = (strchr(t, '$') ? expand_var(t) : strdup(t));
                if (!expanded) { expanded = strdup(t); }
                int tmpfd = open(expanded, O_RDONLY);
                if (tmpfd < 0) {
                  
                    char msg[512];
                    int len = snprintf(msg, sizeof(msg),
                                       "cannot access %s: %s\n",
                                       expanded, strerror(errno));
                    if (fd_err >= 0) {
                      
                        ssize_t w = write(fd_err, msg, (size_t)len);
                        (void)w;
                    } else {
                        /* print to shell stderr */
                        fprintf(stderr, "%s", msg);
                    }
                    free(expanded);
                    open_failed = 1;
                    break;
                } else {
                    if (fd_in >= 0) close(fd_in);
                    fd_in = tmpfd;
                    if (in_file_str) free(in_file_str);
                    in_file_str = expanded;
                }
                continue;
            }
            
            char *tok = read_token(&p);
            if (tok && argc_exec < MAX_ARGS-1) {
                if (strchr(tok, '$')) {
                    char *exp = expand_var(tok);
                    argv_exec[argc_exec++] = exp;
                    alloced_args[alloced_count++] = exp;
                } else {
                    argv_exec[argc_exec++] = tok;
                }
            } else {
                argc_exec = 0;
                break;
            }
        } 
        if (argc_exec == 0) {
           
            if (fd_in >= 0) { close(fd_in); fd_in = -1; }
            if (fd_out >= 0) { close(fd_out); fd_out = -1; }
            if (fd_err >= 0) { close(fd_err); fd_err = -1; }
            for (int i = 0; i < alloced_count; ++i) free(alloced_args[i]);
            if (in_file_str) { free(in_file_str); in_file_str = NULL; }
            if (out_file_str) { free(out_file_str); out_file_str = NULL; }
            if (err_file_str) { free(err_file_str); err_file_str = NULL; }
            continue;
        }

        argv_exec[argc_exec] = NULL;

        if (open_failed) {
            /* cleanup */
            if (fd_in >= 0) { close(fd_in); fd_in = -1; }
            if (fd_out >= 0) { close(fd_out); fd_out = -1; }
            if (fd_err >= 0) { close(fd_err); fd_err = -1; }
            for (int i = 0; i < alloced_count; ++i) free(alloced_args[i]);
            if (in_file_str) { free(in_file_str); in_file_str = NULL; }
            if (out_file_str) { free(out_file_str); out_file_str = NULL; }
            if (err_file_str) { free(err_file_str); err_file_str = NULL; }
            last_status = 1;
            continue;
        }

        pid_t pid = fork();
        if (pid < 0) {
            perror("fork");
            /* cleanup */
            if (fd_in >= 0) { close(fd_in); fd_in = -1; }
            if (fd_out >= 0) { close(fd_out); fd_out = -1; }
            if (fd_err >= 0) { close(fd_err); fd_err = -1; }
            for (int i = 0; i < alloced_count; ++i) free(alloced_args[i]);
            if (in_file_str) { free(in_file_str); in_file_str = NULL; }
            if (out_file_str) { free(out_file_str); out_file_str = NULL; }
            if (err_file_str) { free(err_file_str); err_file_str = NULL; }
            last_status = 1;
            continue;
        }

        if (pid == 0) {
            /* child */
            if (fd_in >= 0) { dup2(fd_in, STDIN_FILENO); close(fd_in); }
            if (fd_out >= 0) { dup2(fd_out, STDOUT_FILENO); close(fd_out); }
            if (fd_err >= 0) { dup2(fd_err, STDERR_FILENO); close(fd_err); }
            execvp(argv_exec[0], argv_exec);
            _exit(127);
        } else {
            /* parent */
            if (fd_in >= 0) { close(fd_in); fd_in = -1; }
            if (fd_out >= 0) { close(fd_out); fd_out = -1; }
            if (fd_err >= 0) { close(fd_err); fd_err = -1; }

            int status;
            if (waitpid(pid, &status, 0) > 0) {
                if (WIFEXITED(status)) last_status = WEXITSTATUS(status);
                else last_status = 1;
            }

            for (int i = 0; i < alloced_count; ++i) free(alloced_args[i]);
            alloced_count = 0;
            if (in_file_str) { free(in_file_str); in_file_str = NULL; }
            if (out_file_str) { free(out_file_str); out_file_str = NULL; }
            if (err_file_str) { free(err_file_str); err_file_str = NULL; }
        }
    } 

    return last_status;
}
