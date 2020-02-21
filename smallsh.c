#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>
#include <sys/types.h>
#include <wait.h>
#include <signal.h>
#include <error.h>

#define MAX_ARGS 5 // 512
#define MAX_CHARS 2048
#define DIR_LEN 512
#define ARG_LEN 16 //256
#define MAX_NUM_PROC 128
int errno;
void printStringArr(char** arr, int numArgs);

void catchSIGINT(int signo)
{
	char* message = "SIGINT. Use CTRL-Z to Stop.\n";
	write(STDOUT_FILENO, message, 28);
}


int main() {
	int numCharsEntered = -5;
	// strcpy(args[0], "Hi");
	size_t bufferSize = MAX_CHARS;
	pid_t processList[MAX_NUM_PROC] = { -2 };
	pid_t groupid;
	int processCount = 0;
	int exitCode = 0;
	int execCode = -9;
	//// handle signal getline
	//struct sigaction SIGINT_action = { 0 };
	//SIGINT_action.sa_handler = catchSIGINT;
	//sigfillset(&SIGINT_action.sa_mask);
	////SIGINT_action.sa_flags = SA_RESTART;
	//sigaction(SIGINT, &SIGINT_action, NULL);

	while (true) {
		char args[MAX_ARGS][ARG_LEN] = { '\0' };
		int argc = 0;
		char* buffer = NULL;



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
				strcpy(args[argc], token);
				token = strtok(NULL, " ");
				argc++;
			}
			// built-in commands
			if (strcmp(args[0], "exit") == 0) {
				// send sigkill to all children
				killpg(groupid, 9);

				exit(0);
			}
			else if (strcmp(args[0], "status") == 0) {
				printf("exit value 0\n");
			}
			else if (strcmp(args[0], "cd") == 0) {
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
			}
			else {
				// non built-in cmds
				int childExitMethod = -5;


				// background
				if (strcmp(args[argc - 1], "&") == 0) {
					printf("Background process\n");
				}
				// foreground
				else {
					//printf("Foreground process\n");
					pid_t spawnid = -5;
					char* temp[] = { "ls", "-lap", NULL, NULL };

					spawnid = fork();
					switch (spawnid) {
					case -1:
						perror("Fork Error!\n");
						exit(1);
						break;
					case 0:;
						//printf("Child Process.. \n");
						execCode = execvp("ls", temp);
						perror("Invalid Command! Command can't be executed\n");
						exit(1);
						break;
					default:;
						// storing children ID list if child exec-ed successfully
						//if (execCode != -1) {
							/*processList[processCount] = spawnid;
							processCount = processCount + 1;*/
						groupid = getpgid(spawnid);
						printf("group id: %d\n", groupid);
						int childPid_actual = waitpid(spawnid, &childExitMethod, 0);



						//printf("Child terminated.\n");

						// get exit status of the child
						if (WIFEXITED(childExitMethod) != 0) {
							printf("Child exited normally\n");
							int exitStatus = WEXITSTATUS(childExitMethod);
							printf("exit status: %d\n", exitStatus);
						}

						else if (WIFSIGNALED(childExitMethod) != 0) {
							printf("Child terminated by signal\n");
							int termSignal = WTERMSIG(childExitMethod);
							printf("exit status: %d\n", termSignal);

						}



						break;
						//}
					}

				}
			}
		}
		free(buffer);
		buffer = NULL;
	}

	return 0;
}

void printStringArr(char** arr, int numArgs) {
	int i;
	for (i = 0; i < numArgs; i++) {
		printf("Arg #%d: %s\n", i, arr[i]);
	}
}
