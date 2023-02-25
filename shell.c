#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/wait.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/types.h>


#define MAX_LINE_LENGTH 255
#define MAX_ARG_LENGTH 100
#define MAX_ARGS (MAX_LINE_LENGTH / 2 + 1)

char* previous_command = NULL;
char** parse_command(char* line);
char *strdup(const char *s);
char *args[MAX_ARGS];

void execute_command(char **args);
void change_directory(char **args);
void execute_script(char **args);
void print_previous_command();
void print_help();

int main(int argc, char *argv[]) {
    char line[MAX_LINE_LENGTH + 1];
    int status;

    printf("Welcome to mini-shell.\n");

    while (1) {
        printf("shell $ ");
        fflush(stdout);

        if (fgets(line, sizeof(line), stdin) == NULL) {
            printf("Bye bye.\n");
            exit(0);
        }

        line[strcspn(line, "\n")] = '\0'; // remove trailing newline

        char* commands[MAX_ARGS];
        int num_commands = 0;
        char* command = strtok(line, ";"); // split line into commands using ';'

        while (command != NULL) {
            commands[num_commands] = strdup(command);
            num_commands++;
            command = strtok(NULL, ";");
        }

        for (int i = 0; i < num_commands; i++) {
            int arg_count = 0;
            char *args[MAX_ARGS];
            char *token = strtok(commands[i], " ");

            while (token != NULL) {
                if (token[0] == '"') {
                    char *end_quote = strchr(token + 1, '"');
                    if (end_quote == NULL) {
                        printf("Error: unmatched double quote.\n");
                        break;
                    }
                    int len = end_quote - token - 1;
                    args[arg_count] = malloc(len + 1);
                    strncpy(args[arg_count], token + 1, len);
                    args[arg_count][len] = '\0';
                    arg_count++;
                    token = end_quote + 1;
                } else {
                    args[arg_count] = strdup(token);
                    arg_count++;
                    token = strtok(NULL, " ");
                }
            }

            args[arg_count] = NULL;

            if (arg_count == 0) {
                continue; // empty command line
            }

            if (strcmp(args[0], "exit") == 0) {
                printf("Bye bye.\n");
                exit(0);
            }

            if (strcmp(args[0], "cd") == 0) {
                change_directory(args);
            } else if (strcmp(args[0], "source") == 0) {
                execute_script(args);
            } else if (strcmp(args[0], "prev") == 0) {
                print_previous_command();
            } else if (strcmp(args[0], "help") == 0) {
                print_help();
            } else {
                execute_command(args);
            }

            for (int i = 0; i < arg_count; i++) {
                free(args[i]);
            }
        }

        for (int i = 0; i < num_commands; i++) {
            free(commands[i]);
        }
    }

    return 0;
}


void change_directory(char **args) {
    if (args[1] == NULL) {
        fprintf(stderr, "cd: expected argument to \"cd\"\n");
    } else {
        if (chdir(args[1]) != 0) {
            perror("cd");
        }
    }
}

void execute_script(char **args) {
    if (args[1] == NULL) {
        fprintf(stderr, "source: expected argument to \"source\"\n");
        return;
    }

    FILE *fp = fopen(args[1], "r");
    if (fp == NULL) {
        perror("source");
        return;
    }

    char line[MAX_LINE_LENGTH];
    while (fgets(line, MAX_LINE_LENGTH, fp) != NULL) {
        line[strcspn(line, "\n")] = '\0'; // remove trailing newline
        execute_command(parse_command(line));
    }

    fclose(fp);
}


void print_previous_command() {
    if (previous_command == NULL) {
        printf("No previous command.\n");
    } else {
        printf("%s\n", previous_command);
        char **args = parse_command(previous_command);
        execute_command(args);
        free(args);
    }
}


void print_help() {
    printf("Mini-shell built-in commands:\n");
    printf("cd [path]: Change the current working directory.\n");
    printf("source [filename]: Execute a script.\n");
    printf("prev: Print the previous command and execute it again.\n");
    printf("help: Print this help message.\n");
}

char **parse_command(char *line) {
    char **args = malloc(MAX_ARGS * sizeof(char *));
    char *arg = strtok(line, " \n");
    int i = 0;

    while (arg != NULL && i < MAX_ARGS) {
        args[i] = malloc(MAX_ARG_LENGTH * sizeof(char));
        strcpy(args[i], arg);
        i++;
        arg = strtok(NULL, " \n");
    }

    args[i] = NULL;

    return args;
}

void execute_command(char **args) {
    if (args[0] == NULL) {
        return; // empty command line
    }

    if (strcmp(args[0], "cd") == 0) {
        change_directory(args);
        return;
    }

    if (strcmp(args[0], "source") == 0) {
        execute_script(args);
        return;
    }

    if (strcmp(args[0], "prev") == 0) {
        print_previous_command();
        return;
    }

    if (strcmp(args[0], "help") == 0) {
        print_help();
        return;
    }

    char *input_file = NULL;
    char *output_file = NULL;
    int input_fd = STDIN_FILENO;
    int output_fd = STDOUT_FILENO;

    for (int i = 1; args[i] != NULL; i++) {
        if (strcmp(args[i], "<") == 0) {
            if (args[i+1] != NULL) {
                input_file = args[i+1];
                input_fd = open(input_file, O_RDONLY);
                if (input_fd < 0) {
                    printf("Could not open input file %s\n", input_file);
                    return;
                }
            }
            args[i] = NULL;
            i++;
        } else if (strcmp(args[i], ">") == 0) {
            if (args[i+1] != NULL) {
                output_file = args[i+1];
                output_fd = open(output_file, O_WRONLY | O_CREAT | O_TRUNC, 0644);
                if (output_fd < 0) {
                    printf("Could not open output file %s\n", output_file);
                    return;
                }
            }
            args[i] = NULL;
            i++;
        }
    }

    pid_t pid = fork();

    if (pid == 0) { // child process
        if (input_file != NULL) {
            dup2(input_fd, STDIN_FILENO);
            close(input_fd);
        }

        if (output_file != NULL) {
            dup2(output_fd, STDOUT_FILENO);
            close(output_fd);
        }

        execvp(args[0], args);
        printf("%s: command not found\n", args[0]);
        exit(1);
    } else if (pid > 0) { // parent process
        wait(NULL);
        // set previous_command to the current command
        free(previous_command);
        previous_command = strdup(args[0]);
        for (int i = 1; args[i] != NULL; i++) {
            previous_command = realloc(previous_command, strlen(previous_command) + strlen(args[i]) + 2);
            strcat(previous_command, " ");
            strcat(previous_command, args[i]);
        }
    }
}


