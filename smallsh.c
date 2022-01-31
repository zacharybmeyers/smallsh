#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/wait.h>
#include <fcntl.h>
#include <signal.h>

int EXIT_STATUS = 0;

void print_exit_status() {
    // exited normally
    if (WIFEXITED(EXIT_STATUS)) {
        printf("exit value %d\n", WEXITSTATUS(EXIT_STATUS));
        fflush(stdout);
    }
    // exited due to signal
    else if (WIFSIGNALED(EXIT_STATUS)) {
        printf("terminated by signal %d\n", WTERMSIG(EXIT_STATUS));
        fflush(stdout);
    }
}

void check_bg_processes() {
    pid_t term_pid;
    int child_status;

    // check for any terminated background processes before returning control to user
    // (use while loop to catch multiple processes)
    while ( (term_pid = waitpid(-1, &child_status, WNOHANG)) > 0 ) {
        printf("background pid %d is done: ", term_pid);
        fflush(stdout);

        EXIT_STATUS = child_status;
        print_exit_status();
    }
}

// custom handler for Ctrl-C
void sigint_handler(int signo) {
    EXIT_STATUS = signo;
    print_exit_status();
    // char *message = "terminated by signal 2\n";
    // write(STDOUT_FILENO, message, 23);
    // fflush(stdout);
    // NEED TO ACTUALLY KILL CHILD PROCESS SOMEHOW?
}

/*
    COMMAND LINE STRUCT AND ALL METHODS
        --print_command_line()
        --create_command_line()
        --free_command_line()
        --execute_command()
*/

typedef struct CommandLine {
    char *args[512];
    char *input_file;
    char *output_file;
    int background;
    int arg_count;
} CommandLine;

// helper function to print contents of CommandLine struct
void print_command_line(CommandLine *cmd) {
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
    fflush(stdout);
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

    // start tokenizing line, make copy as prev_token for later use
    char *saveptr;
    char *token = strtok_r(line, " ", &saveptr);
    char *prev_token = strdup(token);

    // create flags to track special symbols and index of arguments
    int input_flag = 0, output_flag = 0, i = 0;  

    // iterate while token != NULL
    do {        
        // save token as input file
        if (input_flag) {
            cmd->input_file = strdup(token);
            // reset special symbol flag
            input_flag = 0;
        }
        // save token as output file
        else if (output_flag) {
            cmd->output_file = strdup(token);
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
            // check for variable expansion in token
            char expanded[256];
            int offset = 0;
            for (int i = 0; i < strlen(token); i++) {
                // if $$ is the next pair
                if (token[i] == '$' && token[i+1] == '$') {
                    // copy pid into expanded
                    sprintf(expanded + offset, "%d", getpid());
                    // update offset by new length
                    offset = strlen(expanded);
                    // advance i to next pair
                    i++;
                } 
                // otherwise, copy char into expanded, increase offset by 1 char
                else {
                    sprintf(expanded + offset, "%c", token[i]);
                    offset++;
                }
            }

            // point token to expanded version
            token = expanded;

            // copy argument (whether expanded or not)
            cmd->args[i] = strdup(token);
            i++;    

        }

        // update prev_token
        free(prev_token);
        prev_token = strdup(token);

    } while ( (token = strtok_r(NULL, " ", &saveptr)) );

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
    if (cmd->input_file) {
        free(cmd->input_file);
    }
    if (cmd->output_file) {
        free(cmd->output_file);
    }
    free(cmd);
}

void execute_command(CommandLine *cmd, struct sigaction si_action) {
    int child_status, result;
    
    pid_t child_pid = fork();

    switch(child_pid) {
        case -1:
            perror("fork()\n");
            exit(1);
            break;
        case 0:
            // in child process

            // foreground process: allow Ctrl-C
            if (!cmd->background) {
                si_action.sa_handler = SIG_DFL;
                sigaction(SIGINT, &si_action, NULL);
            }

            // handle input
            if (cmd->input_file != NULL) {                
                int sourceFD = open(cmd->input_file, O_RDONLY);
                if (sourceFD == -1) {
                    printf("cannot open %s for input\n", cmd->input_file);
                    fflush(stdout);
                    exit(1);
                }

                // redirect stdin to source file
                result = dup2(sourceFD, 0);
                if (result == -1) {
                    printf("failed source on dup2()\n");
                    fflush(stdout);
                    exit(1);
                }
                
                // close fd
                fcntl(sourceFD, F_SETFD, FD_CLOEXEC);
            }

            // handle output
            if (cmd->output_file != NULL) {
                int targetFD = open(cmd->output_file, O_WRONLY | O_CREAT | O_TRUNC, 0644);
                if (targetFD == -1) {
                    printf("cannot open %s for output\n", cmd->output_file);
                    fflush(stdout);
                    exit(1);
                }
                
                // redirect stdout to target file
                result = dup2(targetFD, 1);
                if (result == -1) {
                    printf("failed target on dup2()\n");
                    fflush(stdout);
                    exit(1);
                }

                // close fd
                fcntl(targetFD, F_SETFD, FD_CLOEXEC);
            }

            // execute command
            if (execvp(cmd->args[0], cmd->args)) {
                printf("%s: no such file or directory\n", cmd->args[0]);
                fflush(stdout);
                exit(1);
            }
            break;
        default:
            // in parent process, wait for child to terminate

            // BACKGROUND EXECUTE
            if (cmd->background) {
                // display bg process start to user
                printf("background pid is %d\n", child_pid);
                fflush(stdout);
                child_pid = waitpid(child_pid, &child_status, WNOHANG);                
            }
            // FOREGROUND EXECUTE
            else {
                child_pid = waitpid(child_pid, &child_status, 0);
                // store exit status
                EXIT_STATUS = child_status;
            }
        }
        // // check for background processes that may have exited due to this command
        // check_bg_processes();
}

int main() {
    printf("welcome to smallsh!\n");
    fflush(stdout);

    // ignore ^C by default
    struct sigaction si_action = {};
    si_action.sa_handler = SIG_IGN;
    sigfillset(&si_action.sa_mask);
    si_action.sa_flags = 0;
    sigaction(SIGINT, &si_action, NULL);
    
    int runsh = 1;

    do {
        // check for background process that have terminated without a signal
        if (!WIFSIGNALED(EXIT_STATUS)) {
            check_bg_processes();
        }

        // set max prompt length
        int prompt_len = 2048;
        char line[prompt_len];
        
        // start prompt with colon
        printf(": ");
        fflush(stdout);
        fgets(line, prompt_len, stdin);
        
        // remove newline char
        for (int i = 0; i < prompt_len; i++) {
            if (line[i] == '\n') {
                line[i] = '\0';
                break;
            }
        }

        // if line starts empty, with whitespace, or with a comment, continue
        if (line[0] == '\0' || line[0] == ' ' || line[0] == '#') {
            continue;
        }

        // break command into pieces and store in struct
        CommandLine *cmd = create_command_line(line);
        //print_command_line(cmd);

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
            print_exit_status();
        }
        // NON-BUILT IN COMMANDS
        else {
            execute_command(cmd, si_action);
        }

        free_command_line(cmd);

    } while (runsh);

    return 0;
}