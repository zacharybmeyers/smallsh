/* 

Name: Zachary Meyers
Date: 2022-02-05
Description: This program creates an interactive small shell (smallsh) to practice 
            command execution, signal handling, i/o redirection, and variable expansion in C.
            A linked list implementation is used to store/cleanup background processes, and
            a CommandLine struct is used to parse and store user entered commands. 

*/

#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/wait.h>
#include <fcntl.h>
#include <signal.h>
#include <errno.h>

// global variables
int EXIT_STATUS = 0;
volatile sig_atomic_t flag = 0;

// function declarations
void print_exit_status();

/*
    GLOBAL LINKED LIST FOR BACKGROUND PROCESSES AND ALL METHODS
        --add_bg_process()
        --remove_bg_head()
        --remove_bg_tail()
        --remove_bg_process()
        --free_bg_list()
        --print_bg_list()
*/

typedef struct BgProc {
    int bg_pid;
    struct BgProc *prev;
    struct BgProc *next;
} BgProc;

BgProc *bg_head = NULL;
BgProc *bg_tail = NULL;

// add node to linked list
void add_bg_process(int pid) {
    // create new node
    BgProc *new_node = malloc(sizeof(BgProc));
    new_node->bg_pid = pid;
    new_node->prev = NULL;
    new_node->next = NULL;
    
    // if no head, set head and tail
    if (bg_head == NULL && bg_tail == NULL) {
        bg_head = new_node;
        bg_tail = new_node;
    } 
    // otherwise, add to end of list
    else {
        // end of list, add new node
        bg_tail->next = new_node;
        new_node->prev = bg_tail;
        bg_tail = new_node;
    }
}

// remove head of linked list
void remove_bg_head() {
    // if head is only node
    if (bg_head->next == NULL && bg_head->prev == NULL) {
        free(bg_head);
        bg_head = NULL;
        bg_tail = NULL;
    } 
    // otherwise, move head to next node
    else {
        BgProc *temp;
        temp = bg_head;
        bg_head = temp->next;
        bg_head->prev = NULL;

        temp->next = NULL;
        free(temp);
    }
}

// remove tail of linked list
void remove_bg_tail() {
    // if tail is only node
    if (bg_tail->next == NULL && bg_tail->prev == NULL) {
        free(bg_tail);
        bg_tail = NULL;
        bg_head = NULL;
    }
    // otherwise, move tail to previous node
    else {
        BgProc *temp;
        temp = bg_tail;
        bg_tail = temp->prev;
        bg_tail->next = NULL;

        temp->prev = NULL;
        free(temp);
    }    
}

// remove node from linked list
void remove_bg_process(int pid) {
    // no processes to remove
    if (bg_head == NULL && bg_tail == NULL) {
        return;
    }
    // if head node
    if (bg_head->bg_pid == pid) {
        remove_bg_head();
    }
    // if tail node
    else if (bg_tail->bg_pid == pid) {
        remove_bg_tail();
    }
    // if middle node
    else {
        // get current node
        BgProc *curr;
        curr = bg_head;

        // loop until curr node is the removal node
        while (curr->bg_pid != pid) {
            curr = curr->next;
        }

        // create pointers to left and right of curr
        BgProc *left;
        left = curr->prev;
        BgProc *right;
        right = curr->next;

        // point over removal node
        left->next = right;
        right->prev = left;

        // free removal node
        free(curr);
        curr->prev = NULL;
        curr->next = NULL;
    }
    
}

// helper function to free linked list
void free_bg_list() {
    // get head
    BgProc *curr;
    curr = bg_head;

    // if there's at least one node
    while (curr) {
        // store next
        BgProc *next;
        next = curr->next;

        // kill process
        kill(curr->bg_pid, SIGTERM);

        // free curr node
        free(curr);
        curr->prev = NULL;
        curr->next = NULL;
        // update curr
        curr = next;
    }
}

// helper function to print contents of linked list
void print_bg_list() {
    BgProc *curr;
    curr = bg_head;

    printf("NULL ");
    while (curr) {
        printf("<- %d -> ", curr->bg_pid);
        curr = curr->next;
    }
    printf("NULL\n");
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

// function takes a valid line and unpacks it into a CommandLine struct
CommandLine *create_command_line(char *line) {
    // initialize new struct and fields
    CommandLine *cmd = malloc(sizeof(CommandLine));
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

    // if last token is background indicator
    if (strcmp(prev_token, "&") == 0) {
        // if NOT foreground only mode, set background status
        if (!flag) {
            cmd->background = 1;
        }
        // regardless of mode: remove & as last argument
        free(cmd->args[i-1]); 
        cmd->args[i-1] = NULL;
        // set argument count
        cmd->arg_count = i-1;
    } 
    // foreground: set argument count
    else {
        cmd->arg_count = i;
    }

    // free prev_token and return cmd struct
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

// execute a non-built in command with the use of fork() and execvp()
void execute_command(CommandLine *cmd) {    
    int child_status, result;

    pid_t child_pid = fork();

    if (child_pid == -1) {
        perror("fork()\n");
        exit(1);
    } 
    else if (child_pid == 0) {
        // in child process

        // ignore Ctrl-Z for foreground and background
        struct sigaction default_sigtstp = {{0}};
        default_sigtstp.sa_handler = SIG_IGN;
        sigfillset(&default_sigtstp.sa_mask);
        default_sigtstp.sa_flags = 0;
        sigaction(SIGTSTP, &default_sigtstp, NULL);

        // foreground process only
        if (!cmd->background) {
            // allow Ctrl-C
            struct sigaction default_sigint = {{0}};
            default_sigint.sa_handler = SIG_DFL;
            sigfillset(&default_sigint.sa_mask);
            default_sigint.sa_flags = 0;
            sigaction(SIGINT, &default_sigint, NULL);
        }

        // handle input redirection
        if (cmd->input_file != NULL) {                
            // open source file
            int sourceFD = open(cmd->input_file, O_RDONLY);

            // failure to open
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

        // handle output redirection
        if (cmd->output_file != NULL) {
            // open target file
            int targetFD = open(cmd->output_file, O_WRONLY | O_CREAT | O_TRUNC, 0644);

            // failure to open
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

        // if bg process and no input: redirect to /dev/null
        if (cmd->background && cmd->input_file == NULL) {
            // open /dev/null
            int sourceFD = open("/dev/null", O_RDONLY);
            
            // failure to open
            if (sourceFD == -1) {
                printf("cannot open /dev/null for input\n");
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

        // if bg process and no output: redirect to /dev/null
        if (cmd->background && cmd->output_file == NULL) {
            // open /dev/null
            int targetFD = open("/dev/null", O_WRONLY);
            
            // failure to open
            if (targetFD == -1) {
                printf("cannot open /dev/null for output\n");
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

    }
    else {
        // in parent process, wait for child to terminate

        // ignore Ctrl-C (SIGINT) by default
        struct sigaction sa_sigint = {{0}};
        sa_sigint.sa_handler = SIG_IGN;
        sigfillset(&sa_sigint.sa_mask);
        sa_sigint.sa_flags = 0;
        sigaction(SIGINT, &sa_sigint, NULL);

        // BACKGROUND EXECUTE
        if (cmd->background) {
            // add bg pid to linked list
            add_bg_process((int)child_pid);

            // display bg process start to user
            printf("background pid is %d\n", child_pid);
            fflush(stdout);

            // wait in background for process to complete
            child_pid = waitpid(child_pid, &child_status, WNOHANG);                
        }
        // FOREGROUND EXECUTE
        else {
            // wait in foreground for process to complete
            child_pid = waitpid(child_pid, &child_status, 0);

            // display only if terminated by Ctrl-C
            if (WIFSIGNALED(child_status) && WTERMSIG(child_status == 2)) {
                printf("terminated by signal %d\n", WTERMSIG(child_status));
                fflush(stdout);
            }

            // update exit status
            EXIT_STATUS = child_status;
        }
    }
}

/*
        MAIN SHELL AND HELPER FUNCTIONS
            --print_exit_status()
            --check_bg_processes()
            --handle_SIGTSTP()
            --main()
*/

// helper function to print the most recent exit status
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

// helper function checks for any terminated background processes
void check_bg_processes() {
    pid_t term_pid;
    int child_status;

    // check for any terminated background processes before returning control to user
    // (use while loop to catch multiple processes)
    while ( (term_pid = waitpid(-1, &child_status, WNOHANG)) > 0 ) {
        // whether exited normally or by signal, show pid is done
        printf("background pid %d is done: ", term_pid);
        fflush(stdout);

        // remove bg pid from linked list
        remove_bg_process((int)term_pid);

        // update and print exit status
        EXIT_STATUS = child_status;
        print_exit_status();
    }
}

// signal handler for Ctrl-Z
void handle_SIGTSTP(int signo) {
    // if flag is set, exit fg only mode, update flag
    if (flag) {
        char const *message = "\nExiting foreground-only mode\n";
        write(STDOUT_FILENO, message, 31);
        flag = 0;
    } 
    // else enter fg only mode, update flag
    else {
        char const *message = "\nEntering foreground-only mode (& is now ignored)\n";
        write(STDOUT_FILENO, message, 51);
        flag = 1;
    }
}

// main shell logic
int main() {
    // ignore Ctrl-Z (SIGTSTP) and handle with handle_SIGTSTP instead
    struct sigaction sa_sigtstp = {{0}};
    sa_sigtstp.sa_handler = handle_SIGTSTP;
    sigfillset(&sa_sigtstp.sa_mask);
    sa_sigtstp.sa_flags = SA_RESTART;
    sigaction(SIGTSTP, &sa_sigtstp, NULL);

    // max prompt length is 2048 characters
    int max_prompt_length = 2048;
    // loop condition
    int runsh = 1;

    while (runsh) {        
        // check for background processes that have terminated before prompting user
        check_bg_processes();

        // flush before each prompt
        fflush(stdout);
        fflush(stdin);

        // start prompt with :
        char const *colon = ": ";
        write(STDOUT_FILENO, colon, 3);

        // prompt user for command line
        char *line = NULL;
        size_t buflen = 0;
        int chartotal = 0;
        chartotal = getline(&line, &buflen, stdin);
        // set newline char from user input to null terminator
        line[chartotal - 1] = '\0';

        // if line starts empty, with whitespace, with a comment, or exceeds max prompt length, continue
        if (line[0] == '\0' || line[0] == ' ' || line[0] == '#' || chartotal > max_prompt_length) {
            free(line);
            continue;
        }

        // break command into pieces and store in struct
        CommandLine *cmd = create_command_line(line);

        // ---BUILT-IN COMMANDS---
        // exit
        if (strcmp(cmd->args[0], "exit") == 0) {
            // remove linked list and kill all bg processes
            free_bg_list();
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
        // ---NON-BUILT IN COMMANDS---
        else {
            execute_command(cmd);
        }

        free_command_line(cmd);
        free(line);

    }

    // remove linked list and kill all bg processes
    free_bg_list();

    return 0;
}