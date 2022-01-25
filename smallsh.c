#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct CommandLine {
    char *command;
    char *args[512];
    char *input_file;
    char *output_file;
    int background;
} CommandLine;

// helper function to print contents of CommandLine struct
void debug_command_line(CommandLine *cmd) {
    printf("cmd->command: %s\n", cmd->command);
    for (int i = 0; i < 512; i++) {
        if (cmd->args[i] == NULL) {
            break;
        }
        printf("cmd->args[%d]: %s\n", i, cmd->args[i]);
    }
    printf("cmd->input_file: %s\n", cmd->input_file);
    printf("cmd->output_file: %s\n", cmd->output_file);
    printf("cmd->background: %d\n", cmd->background);
}

// function takes a valid line and unpacks it into a CommandLine struct
void create_command_line(char *line) {
    CommandLine *cmd = malloc(sizeof(CommandLine));
    // init background to false
    cmd->background = 0;
    
    // start tokenizing line, make copy as prev_token for later use
    char *saveptr;
    char *token = strtok_r(line, " ", &saveptr);
    char *prev_token = calloc(strlen(token) + 1, sizeof(char));
    strcpy(prev_token, token);

    // first token is command
    cmd->command = calloc(strlen(token) + 1, sizeof(char));
    strcpy(cmd->command, token);

    // move to next token
    token = strtok_r(NULL, " ", &saveptr);

    // create flags to track special symbols and index of arguments
    int input_flag = 0, output_flag = 0, i = 0;

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
            cmd->args[i] = calloc(strlen(token) + 1, sizeof(char));
            strcpy(cmd->args[i], token);
            i++;        
        }

        // store prev_token
        prev_token = realloc(prev_token, sizeof(token));
        strcpy(prev_token, token);

        // move to next token
        token = strtok_r(NULL, " ", &saveptr);
    }

    // set background if & was the last token
    if (strcmp(prev_token, "&") == 0) {
        cmd->background = 1;
        // remove & as last argument
        cmd->args[i - 1] = NULL;
    }

    debug_command_line(cmd);
}

void prompt_user() {
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

    printf("you entered: %s\n", line);

    // if line is empty or line is a comment, return
    if (strcmp(line, "") == 0 || line[0] == '#') {
        printf("no command!\n");
        return;
    }

    // not empty, break command into pieces and store in struct
    create_command_line(line);
}

int main() {
    printf("welcome to smallsh!\n");
    prompt_user();
    return 0;
}