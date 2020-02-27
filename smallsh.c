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

#define MAX_ARGS 512
#define MAX_CHARS 2048
#define DIR_LEN 256
#define MAX_NUM_PROC 128

// foreground process PID
pid_t g_foregroundPID = -9;
bool g_fgOnlyMode = false;
int g_exitStatus = -9;
int g_termSignal = -9;

void sigint_handler(int);

void sigtstp_handler(int);

void checkBackground(pid_t *, int *);

bool isBackground(char **, int);

void printArr(char **, int);

void printChars(char *, int);

bool ifExists(char **, int, char *);

int stripArgs(char **, int *, char *);

int getNewArgsSize(char **, int);

void checkExitandTerm(int);

int main() {
  int numCharsEntered = -5;
  size_t bufferSize = MAX_CHARS;
  pid_t bgProcessList[MAX_NUM_PROC] = {-2};
  int bgProcessCount = 0;
  int exitCode = 0;
  int execCode = -9;
  int childExitMethod = -5;

  struct sigaction SIGINT_action = {0}, SIGTSTP_action = {0}, ignore_action = {0};
  // SIGINT declarations
  SIGINT_action.sa_handler = sigint_handler;
  sigfillset(&SIGINT_action.sa_mask);
  SIGINT_action.sa_flags = 0;
  sigaction(SIGINT, &SIGINT_action, NULL);

  // SIGTSTP declarations
  SIGTSTP_action.sa_handler = sigtstp_handler;
  sigfillset(&SIGTSTP_action.sa_mask);
  // SIGTSTP_action.sa_flags = SA_RESTART;
  SIGTSTP_action.sa_flags = 0;

  sigaction(SIGTSTP, &SIGTSTP_action, NULL);

  // ignore
  ignore_action.sa_handler = SIG_IGN;

  while (true) {
    char *args[MAX_ARGS] = {'\0'};
    int argc = 0;
    char *buffer = NULL;
    int i;

    printf(": ");
    fflush(stdout);

    // read input
    numCharsEntered = getline(&buffer, &bufferSize, stdin);
    // handle getline error if there's signal
    if (numCharsEntered == -1)
      clearerr(stdin);

    // remove trailing new line
    buffer[strcspn(buffer, "\n")] = '\0';

    // if input is not a comment nor blank
    if ((strlen(buffer) != 0) && (buffer[0] != '#')) {
      // tokenize input
      char *token = strtok(buffer, " ");

      while (token != NULL) {
        // if in foreground only mode, ignore &
        if (g_fgOnlyMode == true && strcmp(token, "&") == 0) {
          break;
        }

        // expand $$ to PID
        char *pid_pos = strstr(token, "$$");
        if (pid_pos != NULL) {
          int pid_len = sizeof(getpid());
          char pid_str[pid_len];
          int token_len = strlen(token);
          int arg_len = token_len + pid_len - 2;
          // convert pid to string
          sprintf(pid_str, "%d", getpid());

          // create string temp to hold the new arg value
          char temp[arg_len];

          // copy the part of token before $$
          strncpy(temp, token, token_len - strlen(pid_pos));
          // concat pid to temp
          strcat(temp, pid_str);

          // concat 2nd half of token to temp
          strcat(temp, pid_pos + 2);
          // allocate memory for args and copy temp to args
          args[argc] = calloc(arg_len, sizeof(char));
          strcpy(args[argc], temp);
        }

        else {
          args[argc] = calloc(strlen(token), sizeof(char));
          strcpy(args[argc], token);
        }
        token = strtok(NULL, " ");
        argc++;
      }
      // built-in commands
      if (strcmp(args[0], "exit") == 0) {
        // send sigkill to all children
        for (i = 0; i < bgProcessCount; i++) {
          kill(bgProcessList[i], SIGKILL);
        }
        exit(0);
      } else if (strcmp(args[0], "status") == 0) {
        printf("exit status: %d\n", g_exitStatus);
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
          chdir(getenv("HOME"));
          getcwd(s, DIR_LEN);
        }
        // if cd has one argument
        else {
          chdir(args[1]);
        }
      } else {
        // non built-in cmds
        // newArgs = args with IO redirection stripped out
        pid_t spawnid = -5;
        int newArgsSize = getNewArgsSize(args, argc);
        if (isBackground(args, argc) == true)
          newArgsSize = newArgsSize - 1;
        char *newArgs[newArgsSize];
        spawnid = fork();
        switch (spawnid) {
          case -1:
            perror("Fork Error!\n");
            exit(1);
            break;
          case 0:
            // ignore SIGTSTP
            sigaction(SIGTSTP, &ignore_action, NULL);
            int inFD, outFD;
            // check if it's background project
            if (isBackground(args, argc) == true) {
              // strip the & character
              args[argc - 1] = NULL;
              argc--;
              // printf("Background process pid: %d\n", getpid());
              // Redirection IO
              // if there's ">", strip it and its following arg

              if (ifExists(args, argc, ">") == true) {
                outFD = stripArgs(args, &argc, ">");
              } else {
                outFD = open("/dev/null", O_WRONLY, 0644);
              }
              // if there's "<", strip it and its following arg
              if (ifExists(args, argc, "<") == true)
                inFD = stripArgs(args, &argc, "<");
              else
                inFD = open("/dev/null", O_RDONLY, 0644);
            } else  // foreground process
            {
              outFD = stripArgs(args, &argc, ">");
              inFD = stripArgs(args, &argc, "<");
            }

            int inFD_result = dup2(inFD, 0);
            if (inFD_result == -1)
              perror("redirection input error\n");
            int outFD_result = dup2(outFD, 1);
            if (outFD_result == -1)
              perror("redirection output error\n");
            args[argc] = NULL;
            // fcntl(inFD, F_SETFD, FD_CLOEXEC);
            // fcntl(outFD, F_SETFD, FD_CLOEXEC);
            execvp(args[0], args);
            perror("Invalid Command!\n");
            exit(1);

            break;
          default:

            // set foreground PID
            g_foregroundPID = spawnid;
            // If cmd is background process
            if (strcmp(args[argc - 1], "&") == 0) {
              bgProcessList[bgProcessCount] = spawnid;
              bgProcessCount = bgProcessCount + 1;
              printf("Background [%d] - PID %d\n", bgProcessCount, spawnid);
            } else {
              // printf("spawnid: %d\n", spawnid);
              int childPid_actual = waitpid(spawnid, &childExitMethod, 0);
              checkExitandTerm(childExitMethod);
            }
            break;
        }
      }
    }
    // check all bg processes
    for (i = 0; i < bgProcessCount && bgProcessCount > 0; i++) {
      int bgStatus = waitpid(bgProcessList[0], &childExitMethod, WNOHANG);
      if (bgStatus != -1 && bgStatus != 0) {
        printf("Background [%d] done - PID: %d\n", bgProcessCount, bgStatus);
        printf("Exit status: %d\n", exitCode);
        fflush(stdout);
        bgProcessList[i] = 0;
        bgProcessCount = bgProcessCount - 1;
        checkExitandTerm(childExitMethod);
      }
    }
    free(buffer);
    buffer = NULL;
  }

  return 0;
}

bool isBackground(char **args, int argc) {
  if (strcmp(args[argc - 1], "&") == 0)
    return true;
  else
    return false;
}

void printArr(char **arr, int len) {
  int i;
  for (i = 0; i < len; i++) {
    printf("i=%d  ->  %s\n", i, arr[i]);
  }
}

bool ifExists(char **arr, int len, char *str) {
  int i;
  for (i = 0; i < len; i++) {
    if (strcmp(arr[i], str) == 0)
      return true;
  }
  return false;
}

int stripArgs(char **args, int *argc, char *ioChar) {
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
  } else {
    newFD = strcmp(ioChar, ">") == 0 ? 1 : 0;
  }

  return newFD;
}

int getNewArgsSize(char *args[MAX_ARGS], int argc) {
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

void sigint_handler(int sigNum) {
  char *message = "Terminated by signal 2\n";

  write(STDERR_FILENO, message, 23);
  kill(g_foregroundPID, SIGTERM);
}

void sigtstp_handler(int sigNum) {
  // kill(g_foregroundPID, 0) != 0 &&
  // if PID is dead
  if (g_fgOnlyMode == false) {
    write(STDERR_FILENO, "Entering foreground-only mode (& is now ignored)\n", 49);
    g_fgOnlyMode = true;
  }
  // if PID is still alive
  else if (g_fgOnlyMode == true) {
    write(STDERR_FILENO, "Exiting foreground-only mode\n", 29);
    g_fgOnlyMode = false;
  }
}

void printChars(char *arr, int len) {
  int i;
  for (i = 0; i < len; i++) {
    printf("i=%d  ->  %c\n", i, arr[i]);
  }
}

void checkExitandTerm(int childExitMethod) {
  if (WIFEXITED(childExitMethod) != 0) {
    g_exitStatus = WEXITSTATUS(childExitMethod);
  }
  // if child terminated by a signal
  else if (WIFSIGNALED(childExitMethod) != 0) {
    g_termSignal = WTERMSIG(childExitMethod);
    printf("Terminated by signal %d\n", g_termSignal);
  }
}