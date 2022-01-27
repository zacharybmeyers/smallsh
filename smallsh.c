#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/wait.h>

typedef struct CommandLine {
    char *args[512];
    char *input_file;
    char *output_file;
    int background;
    int arg_count;
} CommandLine;

// helper function to print contents of CommandLine struct
void debug_command_line(CommandLine *cmd) {
    for (int i = 0; i < 512; i++) {
        if (cmd->args[i] == NULL) {
            break;
        }
        printf("cmd->args[%d]: %s\n", i, cmd->args[i]);
    }
    printf("cmd->input_file: %s\n", cmd->input_file);
    printf("cmd->output_file: %s\n", cmd->output_file);
    printf("cmd->background: %d\n", cmd->background);
    printf("cmd->arg_count: %d\n", cmd->arg_count);
}

// function takes a valid line and unpacks it into a CommandLine struct
CommandLine *create_command_line(char *line) {
    CommandLine *cmd = malloc(sizeof(CommandLine));
    // init struct fields
    for (int i = 0; i < 512; i++) {
        cmd->args[i] = NULL;
    }
    cmd->input_file = NULL;
    cmd->output_file = NULL;
    cmd->background = 0;
    cmd->arg_count = 0;

    // convert pid to string for variable expansion
    char str_pid[32];
    sprintf(str_pid, "%d", getpid());
    
    // start tokenizing line, make copy as prev_token for later use
    char *saveptr;
    char *token = strtok_r(line, " ", &saveptr);
    char *prev_token = calloc(strlen(token) + 1, sizeof(char));
    strcpy(prev_token, token);

    // create flags to track special symbols and index of arguments
    int input_flag = 0, output_flag = 0, i = 0;  

    // get next token, iterate while token != NULL
    while (token != NULL) {        
        // save token as input file
        if (input_flag) {
            cmd->input_file = calloc(strlen(token) + 1, sizeof(char));
            strcpy(cmd->input_file, token);
            // reset special symbol flag
            input_flag = 0;
        }
        // save token as output file
        else if (output_flag) {
            cmd->output_file = calloc(strlen(token) + 1, sizeof(char));
            strcpy(cmd->output_file, token);
            // reset special symbol flag
            output_flag = 0;
        }
        // set special symbol flags
        else if (strcmp(token, "<") == 0) {
            input_flag = 1;
        }
        else if (strcmp(token, ">") == 0) {
            output_flag = 1;
        }
        // otherwise, save token as an argument
        else {
            // variable expansion: if $$ set smallsh pid
            char *new_token = calloc(strlen(token))
            for (int i = 0; i < strlen(token); i++) {
                if (token[i] == '$' && token[i+1] == '$') {

                }
            }
            if (strcmp(token, "$$") == 0) {
                cmd->args[i] = calloc(strlen(str_pid) + 1, sizeof(char));
                strcpy(cmd->args[i], str_pid);
            } 
            // normal argument
            else {
                cmd->args[i] = calloc(strlen(token) + 1, sizeof(char));
                strcpy(cmd->args[i], token);
            }
            i++;     
        }

        // store prev_token
        // prev_token = realloc(prev_token, sizeof(token));
        free(prev_token);
        prev_token = calloc(strlen(token) + 1, sizeof(char));
        strcpy(prev_token, token);

        // get next token
        token = strtok_r(NULL, " ", &saveptr);

    }

    // set background if & was the last token
    if (strcmp(prev_token, "&") == 0) {
        cmd->background = 1;
        // remove & as last argument
        free(cmd->args[i-1]); 
        cmd->args[i-1] = NULL;
        // set argument count
        cmd->arg_count = i-1;
    } else {
        cmd->arg_count = i;
    }

    free(prev_token);
    return cmd;
}

// helper function frees allocated memory for CommandLine struct
void free_command_line(CommandLine *cmd) {
    for (int i = 0; i < 512; i++) {
        if (cmd->args[i] == NULL) {
            break;
        }
        free(cmd->args[i]);
    }
    free(cmd->input_file);
    free(cmd->output_file);
    free(cmd);
}

void execute_command(CommandLine *cmd) {
    int child_status;

    pid_t spawn_pid = fork();

    switch(spawn_pid) {
    case -1:
        perror("fork()\n");
        exit(1);
        break;
    case 0:
        // in child process
        execvp(cmd->args[0], cmd->args);
        perror("execvp");
        exit(2);
        break;
    default:
        // in parent process, wait for child to terminate
        spawn_pid = waitpid(spawn_pid, &child_status, 0);
    }
}

int main() {
    printf("welcome to smallsh!\n");
    
    int runsh = 1;

    do {
        // set max prompt length
        int prompt_len = 2048;
        char line[prompt_len];
        
        // start prompt with colon
        printf(": ");
        fgets(line, prompt_len, stdin);
        
        // remove newline char
        for (int i = 0; i < prompt_len; i++) {
            if (line[i] == '\n') {
                line[i] = '\0';
                break;
            }
        }

        // if line is empty or line is a comment, return
        if (strcmp(line, "") == 0 || line[0] == '#') {
            continue;
        }

        // not empty, break command into pieces and store in struct
        CommandLine *cmd = create_command_line(line);
        debug_command_line(cmd);

        // BUILT-IN COMMANDS

        // exit
        if (strcmp(cmd->args[0], "exit") == 0) {
            runsh = 0;
        } 
        // cd
        else if (strcmp(cmd->args[0], "cd") == 0) {
            // if there is a directory argument
            if (cmd->args[1]) {
                if (chdir(cmd->args[1]) == -1) {
                    printf("Could not find directory\n");
                    fflush(stdout);
                }
            }
            // else no argument, cd to home
            else {
                chdir(getenv("HOME"));
            }
        }
        // status
        else if (strcmp(cmd->args[0], "status") == 0) {
            break;
        }
        // NON-BUILT IN COMMANDS
        else {
            execute_command(cmd);
        }

        free_command_line(cmd);

    } while (runsh);

    return 0;
}