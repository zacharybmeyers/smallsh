#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/wait.h>
#include <fcntl.h>
#include <signal.h>

int EXIT_STATUS = 0;

// custom handler for Ctrl-C
void sigint_handler(int signo) {
    char *message = "terminated by signal 2\n";
    write(STDOUT_FILENO, message, 23);
    fflush(stdout);
    // NEED TO ACTUALLY KILL CHILD PROCESS SOMEHOW?

    // TODO: CONSIDER STORING A LINKED LIST OF BACKGROUND PIDS
}

/*
    GLOBAL LINKED LIST FOR BACKGROUND PROCESSES AND ALL METHODS
        --add_bg_process()
        --remove_bg_head()
        --remove_bg_tail()
        --remove_bg_process()
        --free_bg_list()
        --print_bg_list()
*/

struct bg_process {
    int bg_pid;
    struct bg_process *prev;
    struct bg_process *next;
};

struct bg_process *bg_head = NULL;
struct bg_process *bg_tail = NULL;

void add_bg_process(int pid) {
    // create new node
    struct bg_process *new_node = malloc(sizeof(struct bg_process));
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

void remove_bg_head() {
    // if head is only node
    if (bg_head->next == NULL && bg_head->prev == NULL) {
        free(bg_head);
        bg_head = NULL;
        bg_tail = NULL;
    } 
    // otherwise, move head to next node
    else {
        struct bg_process *temp;
        temp = bg_head;
        bg_head = temp->next;
        bg_head->prev = NULL;

        temp->next = NULL;
        free(temp);
    }
}

void remove_bg_tail() {
    // if tail is only node
    if (bg_tail->next == NULL && bg_tail->prev == NULL) {
        free(bg_tail);
        bg_tail = NULL;
        bg_head = NULL;
    }
    // otherwise, move tail to previous node
    else {
        struct bg_process *temp;
        temp = bg_tail;
        bg_tail = temp->prev;
        bg_tail->next = NULL;

        temp->prev = NULL;
        free(temp);
    }    
}

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
        struct bg_process *curr;
        curr = bg_head;

        // loop until curr node is the removal node
        while (curr->bg_pid != pid) {
            curr = curr->next;
        }

        // create pointers to left and right of curr
        struct bg_process *left;
        left = curr->prev;
        struct bg_process *right;
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

void free_bg_list() {
    // get head
    struct bg_process *curr;
    curr = bg_head;

    // if there's at least one node
    while (curr) {
        // store next
        struct bg_process *next;
        next = curr->next;
        // free curr node
        free(curr);
        curr->prev = NULL;
        curr->next = NULL;
        // update curr
        curr = next;
    }
}

void print_bg_list() {
    struct bg_process *curr;
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
        --debug_command_line()
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
    free(cmd->input_file);
    free(cmd->output_file);
    free(cmd);
}

void execute_command(CommandLine *cmd) {
    struct sigaction si_action = {};
    // background process: ignore Ctrl-C
    if (cmd->background) {
        si_action.sa_handler = SIG_IGN;
        sigfillset(&si_action.sa_mask);
        si_action.sa_flags = 0;
        sigaction(SIGINT, &si_action, NULL);
    }
    // foreground process: assign signal handler for Ctrl-C
    else {
        si_action.sa_handler = sigint_handler;
        sigfillset(&si_action.sa_mask);
        si_action.sa_flags = 0;
        sigaction(SIGINT, &si_action, NULL);
    }
    
    int child_status, result;
    
    pid_t child_pid = fork();

    switch(child_pid) {
        case -1:
            perror("fork()\n");
            exit(1);
            break;
        case 0:
            // in child process

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
            // // no input specified AND background process... redirect to dev/null
            // else if (cmd->background) {
            //     int sourceFD = open("/dev/null", O_RDONLY);
            //     if (sourceFD == -1) {
            //         printf("cannot open /dev/null for input\n");
            //         fflush(stdout);
            //         exit(1);
            //     }

            //     // redirect stdin to source file
            //     result = dup2(sourceFD, 0);
            //     if (result == -1) {
            //         printf("failed source on dup2()\n");
            //         fflush(stdout);
            //         exit(1);
            //     }

            //     // close fd
            //     fcntl(sourceFD, F_SETFD, FD_CLOEXEC);
            // }

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
            // // no output specified AND background process... redirect to dev/null
            // else if (cmd->background) {
            //     int targetFD = open("/dev/null", O_WRONLY | O_CREAT | O_TRUNC, 0644);
            //     if (targetFD == -1) {
            //         printf("cannot open /dev/null for output\n");
            //         fflush(stdout);
            //         exit(1);
            //     }

            //     // redirect stdin to source file
            //     result = dup2(targetFD, 0);
            //     if (result == -1) {
            //         printf("failed source on dup2()\n");
            //         fflush(stdout);
            //         exit(1);
            //     }

            //     // close fd
            //     fcntl(targetFD, F_SETFD, FD_CLOEXEC);
            // }

            // execute command
            if (execvp(cmd->args[0], cmd->args)) {
                printf("%s: no such file or directory\n", cmd->args[0]);
                fflush(stdout);
                exit(1);
            }
            break;
        default:
            // in parent process, wait for child to terminate

            // execute in background if desired
            if (cmd->background) {
                // display bg process start to user
                printf("background pid is %d\n", child_pid);
                fflush(stdout);

                // add pid to bg_process linked list
                add_bg_process(child_pid);
                print_bg_list();

                child_pid = waitpid(child_pid, &child_status, WNOHANG);
            }
            // execute normally
            else {
                child_pid = waitpid(child_pid, &child_status, 0);
            }

            // // terminated normally
            // if (WIFEXITED(child_status)) {
            //     printf("child %d exited normally with status: %d\n", child_pid, WEXITSTATUS(child_status));
            //     fflush(stdout);
            // }
            // // terminated abnormally
            // else {
            //     printf("child %d exited abnormally due to signal %d\n", child_pid, WTERMSIG(child_status));
            //     fflush(stdout);
            // }
        }
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


    pid_t term_pid;
    int child_status;
    
    int runsh = 1;

    do {
        // check for any terminated background processes before returning control to user
        // (use while loop to catch multiple processes)
        while ( (term_pid = waitpid(-1, &child_status, WNOHANG)) > 0 ) {
            printf("background process %d is complete\n", term_pid);
            fflush(stdout);
            // remove from linked list
            remove_bg_process(term_pid);
            print_bg_list();
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

        // if line starts empty, with whitespace, or with a comment, keep looping
        if (line[0] == '\0' || line[0] == ' ' || line[0] == '#') {
            continue;
        }

        // not empty, break command into pieces and store in struct
        CommandLine *cmd = create_command_line(line);
        //debug_command_line(cmd);

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
            printf("exit value %d\n", EXIT_STATUS);
            fflush(stdout);
        }
        // NON-BUILT IN COMMANDS
        else {
            execute_command(cmd);
        }

        free_command_line(cmd);

    } while (runsh);

    // free background process linked list (if any entries remain)
    free_bg_list();

    return 0;
}