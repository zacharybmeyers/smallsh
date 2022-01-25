#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>

void prompt_user() {
    char *line = NULL;
    size_t len = 0;
    ssize_t nread = 0;
    
    printf(": ");
    nread = getline(&line, &len, stdin);
    line[nread - 1] = '\0';
    printf("you entered: %s\n", line);
    
    free(line);
}

int main() {
    printf("welcome to smallsh!\n");
    prompt_user();
    return 0;
}