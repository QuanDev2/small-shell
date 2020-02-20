#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

#define MAX_ARGS 512
#define MAX_CHARS 2048

void printStringArr(char **arr, int numArgs);

void main() {
    int numCharsEntered = -5;
    // strcpy(args[0], "Hi");
    size_t bufferSize = 2048;
    int i;

    while (true) {
        char args[MAX_ARGS][256] = {'\0'};
        int numArgs = 0;
        char *buffer = NULL;

        printf(": ");
        fflush(stdout);
        numCharsEntered = getline(&buffer, &bufferSize, stdin);
        buffer[strlen(buffer) - 1] = '\0';

        // if input is comment or blank, ignore it || (strlen(buffer) == 0) ||
        if ((strlen(buffer) != 0) && (buffer[0] != '#')) {
            // extract each arg and put it in array
            char *token = strtok(buffer, " ");
            while (token != NULL) {
                strcpy(args[numArgs], token);
                token = strtok(NULL, " ");
                numArgs++;
            }
            // built-in commands
            if (strcmp(args[0], "exit") == 0) {
                exit(0);
            } else if (strcmp(args[0], "status") == 0) {
                printf("exit value 0\n");
            } else if (strcmp(args[0], "cd") == 0) {
                char s[2048];

                getcwd(s, 2048);
                printf("Old dir: %s\n", s);
                chdir(getenv("HOME"));
                getcwd(s, 2048);

                printf("New dir: %s\n", s);
            }
        }
    }
}

void printStringArr(char **arr, int numArgs) {
    int i;
    for (i = 0; i < numArgs; i++) {
        printf("Arg #%d: %s\n", i, arr[i]);
    }
}
