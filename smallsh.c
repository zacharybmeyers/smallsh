#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>

void prompt_user() {
    int prompt_len = 2048;
    char line[prompt_len];
    
    printf(": ");
    fgets(line, prompt_len, stdin);
    
    // remove newline char
    for (int i = 0; i < prompt_len; i++) {
        if (line[i] == '\n') {
            line[i] = '\0';
            break;
        }
        printf("%c\n", line[i]);
    }

    printf("you entered: %s\n", line);
}

int main() {
    printf("welcome to smallsh!\n");
    prompt_user();
    return 0;
}