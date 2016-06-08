// Collin Shoop

/*
 * Homework 1 ??/2014
 * This is a basic shell.
 * Native commands include:
 *    quit/exit: terminate the shell
 *    pwd: print the current working directory via getcwd
 *    cd [path]: change the current working directory to the given path via chdir
 * If a command is not native, it is executed using execvp.
 * If a command run by execvp is invalid/DNE then an error message is displayed.
 */

#include <stdio.h>
#include <sys/types.h> // pid_t -  struct (int)
#include <stdlib.h> // exit and wait
#include <unistd.h> // fork, execlp (required), chdir
#include <string.h> // 'int strncmp(char[], char[], int limit)'

#define MAX_LEN 300 // Arbitrary maximum length of a single shell command. Can be changed.
#define MAX_ARGS 10 // Arbitrary maximum argument count accepted. Can be changed.
#define MAX_WD 300 // Arbitrary maximum string length for 'pwd' result. Can be changed.

void execute(char** args, int arg_count);
void error(char* string);
int parseLine(char str[], char* args[]);
int argEquals(char* arg, char compare[]);

int main() {
    // Startup message
    printf(" _____                     _____ _       _ _ \n");
    printf("|   __|_ _ ___ ___ ___ ___|   __| |_ ___| | |\n");
    printf("|__   | | | . | -_|  _|___|__   |   | -_| | |\n");
    printf("|_____|___|  _|___|_|     |_____|_|_|___|_|_|\n");
    printf("          |_|                            v1  \n");
    printf("Welcome to Super-Shell by Collin Shoop!\n");
    char command_line[MAX_LEN];
    char *args[MAX_ARGS + 1];
    char* prompt = " > ";
    int running = 1;

    while (running) // Main loop: until 'running' is false.
    {
        printf("\n%s", prompt); // display prompt
        fflush(stdout);
        fgets(command_line, MAX_LEN, stdin); // get the line typed

        int arg_count = parseLine(command_line, args); // parse 'command_line' into arguments

        if (arg_count == -1) { // check for MAX_ARGS exceeded
            error("The number of arguments entered exceeds the limit. Ignored.");
            continue;
        } else if (arg_count == 0) { // check for 0 arguments
            continue;
        }

        // Check for a quit/exit.
        if (argEquals(args[0], "quit") || argEquals(args[0], "exit")) {
            printf("Goodbye.");
            running = 0;
        } else {
            execute(args, arg_count); // execute using execvp
        }
    }
    return EXIT_SUCCESS;
}

/*
 * -  Will execute the first 'arg_count' arguments of the 'args' argument array.
 * -  Args must be NULL terminated. That is, args[arg_count]=NULL.
 * -  Supports forking in background using "&" as the last valid argument. That is,
 *    "&" must be the arg_count-1'th argument (if it's included).
 * PRECONDITION: args must have length <= 'MAX_ARGS+1' (to 
 *               account for NULL termination).
 * PRECONDITION: arg_count must be <= 'MAX_ARGS'.
 * PRECONDITION: arg_count must have value < length(args).
 */
void execute(char** args, int arg_count) {
    if (argEquals(args[0], "pwd")) { // execute 'pwd' command
        // Allocate some memory for the path
        char *t_path = malloc(sizeof (char) * MAX_WD);
        // get the path
        if (!getcwd(t_path, MAX_WD)) { // attempt get of native wd
            error("Working directory too long to store.");
        }
        printf("%s\n", t_path);
        free(t_path);
    } else if (argEquals(args[0], "cd")) { // execute 'cd' command
        if (arg_count < 2) {
            error("Not enough arguments. Usage: 'cd [path]'");
        } else {
            if (chdir(args[1])) { // attempt native cd
                error("cd failed");
            }
        }
    } else { // execute using execvp
        pid_t pid;
        pid = fork();
        if (pid == 0) { // child
            if (argEquals(args[arg_count - 1], "&")) {
                // Set this argument to null so that it's not 
                // passed as an argument
                args[arg_count - 1] = NULL;
            }
            int r = execvp(*args, args);
            printf("Invalid command (%i)\n", r);
            exit(EXIT_FAILURE);
        } else if (pid < 0) {
            error("Unable to fork. Command failed.");
        } else {
            if (!argEquals(args[arg_count - 1], "&")) { // if no &
                int status;
                // wait for the correct child to terminate
                while (wait(&status) != pid);
            }
        }
    }
}

/*
 * Parses the given string into an array of char*. Terminates args with a NULL.
 * Argument: str - string to be parsed into arguments
 * Argument: args - char* array to put arguments into (assuming args has length=MAX_ARGS+1)
 * Return: -1 - The number of arguments exceeds the limit MAX_ARGS
 * Return: The number of arguments retrieved
 */
int parseLine(char *str, char** args) {
    // Start the argument parsing
    int argCount = 0;
    char *n_arg = strtok(str, "\r\n ");
    while (argCount < MAX_ARGS && n_arg != NULL) {
        args[argCount++] = n_arg;
        n_arg = strtok(NULL, "\r\n ");
    }
    args[argCount] = NULL; // set the last argument to NULL. not out of bounds.
    if (n_arg == NULL) {
        return argCount; // success
    }
    // argCount == MAX_ARGS with n_arg != NULL means #args > MAX_ARGS
    return -1; // MAX_ARGS exceeded
}

/*
 * Compares the given string and returns whether or not they're the same.
 * Argument: arg - First string to compare
 * Argument: compare - Second string to compare
 * Return: 0 (false) - if the two strings are not equal.
 * Return: 1 (true) - if the two strings are equal.
 * NOTE: Will compare up to MAX_LEN characters.
 */
int argEquals(char* arg, char* compare) {
    return !(strncmp(arg, compare, MAX_LEN));
}

/**
 * Prints the given e with "error format"
 */
void error(char* e) {
    printf(" ** ERROR: '%s' **\n", e);
}
