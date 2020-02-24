#include <error.h>
#include <fcntl.h>
#include <signal.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#include <wait.h>

#define MAX_ARGS 10  // 512
#define MAX_CHARS 2048
#define DIR_LEN 512
#define ARG_LEN 16  //256
#define MAX_NUM_PROC 128

// void catchSIGINT(int signo)
// {
// 	char* message = "SIGINT. Use CTRL-Z to Stop.\n";
// 	write(STDOUT_FILENO, message, 28);
// }

void checkBackground(pid_t*, int*);

void printArr(char** arr, int len) {
  int i;
  for (i = 0; i < len; i++) {
    printf("i=%d  ->  %s\n", i, arr[i]);
  }
}

bool ifExists(char** arr, int len, char* str) {
  int i;
  for (i = 0; i < len; i++) {
    if (strcmp(arr[i], str) == 0) return true;
  }
  return false;
}

int stripArgs(char** args, int* argc, char* ioChar) {
  int stripPos, newFD, ioCharPos;
  if (ifExists(args, *argc, ioChar) == true) {
    for (stripPos = 0; stripPos < *argc; stripPos++) {
      if (strcmp(args[stripPos], ioChar) == 0) {
        int j;
        if (strcmp(ioChar, ">") == 0) {
          newFD = open(args[stripPos + 1], O_WRONLY | O_CREAT | O_TRUNC, 0644);
        } else if (strcmp(ioChar, "<") == 0)
          newFD = open(args[stripPos + 1], O_RDONLY, 0644);
        if (newFD == -1) {
          perror("open() file error\n");
        }
        for (j = stripPos; j < *argc - 2; j++) {
          strcpy(args[j], args[j + 2]);
        }
        break;
      }
    }
    *argc = *argc - 2;

    // return newFD;
  } else {
    newFD = strcmp(ioChar, ">") == 0 ? 1 : 0;
  }
  return newFD;
}

int getNewArgsSize(char* args[MAX_ARGS], int argc) {
  int i;
  int newSize = argc;
  for (i = 0; i < argc; i++) {
    if (strcmp(args[i], "<") == 0)
      newSize = newSize - 2;
    else if (strcmp(args[i], ">") == 0)
      newSize = newSize - 2;
  }
  return newSize + 1;
}

int main() {
  int numCharsEntered = -5;
  // strcpy(args[0], "Hi");
  size_t bufferSize = MAX_CHARS;
  pid_t backgroundProcessList[MAX_NUM_PROC] = {-2};
  int backgroundProcessCount = 0;
  int exitCode = 0;
  int execCode = -9;
  int childExitMethod = -5;
  //// handle signal getline
  //struct sigaction SIGINT_action = { 0 };
  //SIGINT_action.sa_handler = catchSIGINT;
  //sigfillset(&SIGINT_action.sa_mask);
  ////SIGINT_action.sa_flags = SA_RESTART;
  //sigaction(SIGINT, &SIGINT_action, NULL);

  while (true) {
    char* args[MAX_ARGS] = {'\0'};
    int argc = 0;
    char* buffer = NULL;

    // check background processes
    int i;
    // checkBackground(backgroundProcessList, &backgroundProcessCount);
    for (i = 0; i < backgroundProcessCount && backgroundProcessCount > 0; i++) {
      int bgStatus = waitpid(backgroundProcessList[0], &childExitMethod, WNOHANG);
      if (bgStatus != -1 && bgStatus != 0) {
        printf("Background process done - pid: %d\n", bgStatus);
        fflush(stdout);
        backgroundProcessList[i] = 0;
        backgroundProcessCount = backgroundProcessCount - 1;
      }
    }
    printf(": ");
    fflush(stdout);

    numCharsEntered = getline(&buffer, &bufferSize, stdin);
    // handle getline error if there's signal
    if (numCharsEntered == -1)
      clearerr(stdin);

    // remove trailing new line
    //buffer[strlen(buffer) - 1] = '\0';
    buffer[strcspn(buffer, "\n")] = '\0';

    size_t wholeBufferLen = strlen(buffer);
    // if input is comment or blank, ignore it
    if ((strlen(buffer) != 0) && (buffer[0] != '#')) {
      // extract each arg and put it in array
      char* token = strtok(buffer, " ");
      while (token != NULL) {
        args[argc] = calloc(strlen(token), sizeof(char));
        memset(args[argc], '\0', strlen(token));
        strcpy(args[argc], token);
        token = strtok(NULL, " ");
        argc++;
      }
      // built-in commands
      if (strcmp(args[0], "exit") == 0) {
        // send sigkill to all children

        exit(0);
      } else if (strcmp(args[0], "status") == 0) {
        printf("exit value 0\n");
      } else if (strcmp(args[0], "cd") == 0) {
        // if cd has more than two args, then it's invalid
        if (argc > 2) {
          printf("Invalid command\n");
          continue;
        }
        // if only "cd" is typed
        else if (argc == 1) {
          char s[DIR_LEN];
          getcwd(s, DIR_LEN);
          printf("Old dir: %s\n", s);
          chdir(getenv("HOME"));
          getcwd(s, DIR_LEN);

          printf("New dir: %s\n", s);
        }
        // if cd has one argument
        else {
          chdir(args[1]);
        }
      } else {
        // non built-in cmds
        pid_t spawnid = -5;
        const int newArgsSize = getNewArgsSize(args, argc);
        char* newArgs[newArgsSize];
        spawnid = fork();
        switch (spawnid) {
          case -1:
            perror("Fork Error!\n");
            exit(1);
            break;
          case 0:;

            int outFD = stripArgs(args, &argc, ">");
            int inFD = stripArgs(args, &argc, "<");
            int inFD_result = dup2(inFD, 0);
            if (inFD_result == -1) perror("redirection input error\n");
            int outFD_result = dup2(outFD, 1);
            if (outFD_result == -1) perror("redirection output error\n");
            args[argc] = NULL;
            execvp(args[0], args);
            perror("Invalid Command!\n");
            exit(1);

            // if (strcmp(args[argc - 1], "&") == 0) {
            //   // strip the & character
            //   args[argc - 1] = NULL;
            //   printf("Background process pid: %d\n", getpid());

            //   // Redirection IO
            //   int devNullFD = open("/dev/null", O_WRONLY, 0644);
            //   dup2(devNullFD, 1);
            //   dup2(devNullFD, 2);
            // }
            break;
          default:
            // If cmd is background process
            if (strcmp(args[argc - 1], "&") == 0) {
              backgroundProcessList[backgroundProcessCount] = spawnid;
              backgroundProcessCount = backgroundProcessCount + 1;
            } else {
              int childPid_actual = waitpid(spawnid, &childExitMethod, 0);
            }
            //printf("Child terminated.\n");

            // if (bgStatus == 0) {
            // 	// printf("Background is running\n");
            // 	// fflush(stdout);

            // }
            // else if (bgStatus == -1) {
            // 	// perror("Background process has error\n");
            // 	// fflush(stdout);

            // }
            // else {
            // 	printf("Background process terminated, SpawnID - bgStatus: %d - %d\n", backgroundProcessList[0], bgStatus);
            // 	fflush(stdout);

            // }

            // get exit status of the child
            if (WIFEXITED(childExitMethod) != 0) {
              int exitStatus = WEXITSTATUS(childExitMethod);
            }

            else if (WIFSIGNALED(childExitMethod) != 0) {
              int termSignal = WTERMSIG(childExitMethod);
            }

            break;
        }
      }
    }
    free(buffer);
    buffer = NULL;
  }

  return 0;
}

// void checkBackground(pid_t *backgroundProcessList, int *backgroundProcessCount) {
//     int childExitMethod = -5;
//     int i;

//     for (i = 0; i<*backgroundProcessCount && * backgroundProcessCount> 0; i++) {
//         int bgStatus = waitpid(backgroundProcessList[0], &childExitMethod, WNOHANG);
//         if (bgStatus != -1 && bgStatus != 0) {
//             printf("Background process terminated. PID: %d\n", bgStatus);
//             fflush(stdout);
//             backgroundProcessList[i] = -2;
//             *backgroundProcessCount = *backgroundProcessCount - 1;
//         }
//     }
// }
