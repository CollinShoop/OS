// Collin Shoop

/*
 * Homework 2
 * A shell which supports piping, IO redirection, and some signal handling.
 * Native commands include:
 *    quit/exit: terminate the shell
 *    pwd: print the current working directory via getcwd
 *    cd [path]: change the current working directory to the given path via chdir
 * If a command is not native, it is executed using execvp.
 * If a command run by execvp is invalid/DNE then an error message is displayed.
 */

#include <stdio.h>
#include <time.h> // getting current time
#include <signal.h> // signal handling
#include <sys/types.h> // pid_t
#include <stdlib.h> // exit() and wait()
#include <unistd.h> // fork, execvp, chdir
#include <string.h> // 'int strncmp(char[], char[], int limit)'
#include <fcntl.h> // for IO redirection

#define MAX_LEN 300 // Arbitrary maximum length of a single shell command. Can be changed.
#define MAX_ARGS 30 // Arbitrary maximum argument count accepted. Can be changed.
#define MAX_WD 300 // Arbitrary maximum string length for 'pwd' result. Can be changed.
#define MAX_INTMESSAGE_LEN 100 // used in signal_handler(int) as the max message length.
#define PROMPT " > "

// Execution
void exe_line(char** args, int arg_count);
void exe_line2(char** args, int arg_count);
void exe_command(char** args, int arg_count);
void exe_func(char** args, int arg_count);
// Execution helpers
int parse_line(char str[], char* args[]);

// IO Redirection
void redirect_output_name(char* file);
void redirect_input_name(char* file);
void redirect_output_id(int file_id);
void redirect_input_id(int file_id);

// Signal Interruption
void signal_handler(int);

// Output helpers
void prompt();
void error(char* string);

// Misc helpers
int str_equals(char* arg, char compare[]);
int index_of_str(char** args, int arg_count, char* s);
void errorAndTerminate();

int main() {
    // Add signal interrupt handler first
    signal(SIGINT, signal_handler);
    signal(SIGTSTP, signal_handler);

    // Startup message
    printf(" _____                     _____ _       _ _ \n");
    printf("|   __|_ _ ___ ___ ___ ___|   __| |_ ___| | |\n");
    printf("|__   | | | . | -_|  _|___|__   |   | -_| | |\n");
    printf("|_____|___|  _|___|_|     |_____|_|_|___|_|_|\n");
    printf("          |_|                            v2  \n");
    printf("Welcome to Super-Shell by Collin Shoop!\n");
    char command_line[MAX_LEN];
    char *args[MAX_ARGS + 1];
    int running = 1;

    while (running) // Main loop: until 'running' is false.
    {
        prompt();
        fflush(stdout);
        fgets(command_line, MAX_LEN, stdin); // get the line typed

        int arg_count = parse_line(command_line, args); // parse 'command_line' into arguments

        if (arg_count == -1) { // check for MAX_ARGS exceeded
            printf("The number of arguments entered exceeds the limit. Ignored.\n");
            continue;
        } else if (arg_count == 0) { // check for 0 argumentS
            continue;
        }

        // Check for a quit/exit.
        if (str_equals(args[0], "quit") || str_equals(args[0], "exit")) {
            printf("Goodbye.\n");
            running = 0;
        } else {
            exe_line(args, arg_count); // execute using execvp
        }
    }
    return EXIT_SUCCESS;
}

/**
 * Executes the first 'arg_count' arguments of the 'args' argument array.
 * Supports forking in background using "&" as the last valid argument. That is,
 *    if it's included, "&" must be the arg_count'th argument.
 * Supports piping (any number of pipes), input redirection, and output
 *    redirection. Input and output redirection may be used together.
 * PRECONDITIONS:
 * - args must be NULL terminated. That is, args[arg_count]=NULL.
 * - args must have length <= 'MAX_ARGS+1' (to account for NULL termination).
 * - arg_count must be <= 'MAX_ARGS'.
 * - arg_count must have value < length(args).
 */
void exe_line(char** args, int arg_count) {

    /* 
     * Will check for & then fork.
     * Parent: (effectively the shell) 
     *    Wait for child or go back to prompting (if & is the last argument)
     * Child:
     *    Execute the remaining arguments.
     */

    // if & is found it must be removed before forking
    // and flagged
    int amp = 0;
    if (arg_count > 0 && str_equals(args[arg_count - 1], "&")) {
        // Set this argument to null so that it's not 
        // passed as an argument
        args[arg_count - 1] = NULL;
        arg_count--;
        amp = 1;
    }
    pid_t pid = fork();
    if (pid != 0) // parent
    {
        if (!amp) { // if there is NOT an &
            while (wait(NULL) != pid); // wait for the child to finish
        }
        // if there is an & the parent will leave and go back to prompting
    } else { // child
        // execute the remaining arguments
        exe_line2(args, arg_count);
    }

}

/* 
 * Will recursively check for and process end-line IO redirection.
 * Afterwards, any left over arguments are processed by 'exe_command'
 */
void exe_line2(char** args, int arg_count) {

    // Flag for recursing
    int recurse = 0;
    if (arg_count > 2 && str_equals(args[arg_count - 2], ">")) // output redirection
    {
        redirect_output_name(args[arg_count - 1]); // redirect output
        recurse = 1; // flag for recurse
    } else if (arg_count > 2 && str_equals(args[arg_count - 2], "<")) // input redirection
    {
        redirect_input_name(args[arg_count - 1]); // redirect input
        recurse = 1; // flag for recurse
    }
    if (recurse) {
        args[arg_count - 2] = NULL; // get rid of the "<" or ">"
        exe_line2(args, arg_count - 2); // recurse

        // not sure if needed. Added just in case. This process should eventually end in an error
        // and EXIT or be overridden by execvp.
        return;
    }
    // Execute remaining arguments after IO redirection
    exe_command(args, arg_count);
}

/* 
 * Will recursively check for pipes (left to right), handling pipe IO redirection
 * and execution of functions (with their parameters) left to right.
 * -- Pipe, '|' , found:
 *    a pipe is created and the process will fork. The child will redirect 
 *    output and execute any arguments left of the pipe while the parent waits. 
 *    The parent will redirect input and recurse with 'exe_command'.
 * -- No pipe, '|', found:
 *    No more IO redirection is required. The remaining arguments
 *    are executed using 'exe_func'.
 */
void exe_command(char** args, int arg_count) {
    
    int pipeIndex = index_of_str(args, arg_count, "|");
    if (pipeIndex == -1) {
        // No pipes. Execute using exe_func
        exe_func(args, arg_count);
        // exe_func will either terminate this process or override it.
    }
    if (pipeIndex == 0) { // first argument is a pipe. What?
        error("Invalid piping.");
        exit(EXIT_FAILURE);
    }
    
    // A pipe is found!
    
    int fd[2]; //[0]=input, [1]=output
    pipe(fd);
    pid_t pid = fork();
    if (pid == 0) // child
    {
        // Redirect output
        close(fd[0]);
        redirect_output_id(fd[1]);

        // Modify args and arg_count
        args[pipeIndex] = NULL;
        arg_count = pipeIndex;

        // Execute the left half of the command
        exe_func(args, arg_count);
    } else { // parent
        // Redirect input
        close(fd[1]);
        redirect_input_id(fd[0]);

        // Reassign the front and modify arg_count
        args += pipeIndex + 1; // same as: args = &args[pipeIndex + 1];
        arg_count = arg_count - (pipeIndex + 1);
        
        // wait for the correct child to terminate
        while (wait(NULL) != pid);
        
        // Execute the right hand of the pipe recursively
        exe_command(args, arg_count);
    }
}

/*
* Will execute the given arguments.
* Will either override the current process space using execvp
* or execute a local command and terminate the current process.
*/
void exe_func(char** args, int arg_count) {
    
    if (arg_count == 0) {
        errorAndTerminate();
    }

    if (str_equals(args[0], "pwd")) { // execute 'pwd' command
        // Allocate some memory for the path
        char *t_path = malloc(sizeof (char) * MAX_WD);
        // get the path
        if (!getcwd(t_path, MAX_WD)) { // attempt get of native wd
            error("Working directory too long to store.");
        }
        printf("%s\n", t_path);
        free(t_path);
    } else if (str_equals(args[0], "cd")) { // execute 'cd' command
        if (arg_count < 2) {
            error("Not enough arguments. Usage: 'cd [path]'");
        } else {
            if (chdir(args[1])) { // attempt native cd
                error("cd failed");
            }
        }
    } else {
        execvp(*args, args);
        error("Invalid command.");
        exit(EXIT_FAILURE);
    }
    // The process space is not overridden if local commands
    // are executed. So the current process will now terminate.
    exit(EXIT_SUCCESS);
}

/**
 * Parses the given string into an array of char*. Terminates args with a NULL.
 * Argument: str - string to be parsed into arguments
 * Argument: args - char* array to put arguments into (assuming args has length=MAX_ARGS+1)
 * Return: -1 - The number of arguments exceeds the limit MAX_ARGS
 * Return: The number of arguments retrieved
 */
int parse_line(char *str, char** args) {
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

/**
 * Compares the given string and returns whether or not they're the same.
 * Argument: arg - First string to compare
 * Argument: compare - Second string to compare
 * Return: 0 (false) - if the two strings are not equal.
 * Return: 1 (true) - if the two strings are equal.
 * NOTE: Will compare up to MAX_LEN characters.
 */
int str_equals(char* arg, char* compare) {
    return !(strncmp(arg, compare, MAX_LEN));
}

/**
 * Prints the given string, 'e', with the format: 
 * 	 "ERROR: '%s' \n" where s is the string 'e'
 */
void error(char* e) {
    fprintf(stderr, "ERROR: '%s' \n", e);
}

/**
 * Will print an error message and execute
 * exit(EXIT_FAILURE) to stop the current process.
 */
void errorAndTerminate() {
    error("Invalid command.");
    exit(EXIT_FAILURE);
}

/**
 * Display default prompt. This is async friendly.
 */
void prompt() {
    printf("\n%s", PROMPT); // display prompt
}

/**
 * Handles SIGINT and SIGTSTP interrupts.
 * Will respond with a message of the form:
 *    Suspending/Terminating Super-Shell at [time] on [date].
 * when a signal is received.
 * 
 * REFERENCES FOR TIME/DATE:
 * http://www.acm.uiuc.edu/webmonkeys/book/c_guide/2.15.html parts 2.15.9 & 2.15.10
 * http://en.cppreference.com/w/cpp/chrono/c/strftime
 */
void signal_handler(int sig) {
    // Create a date/time message
    time_t now = time(0); // get current time
    char message[MAX_INTMESSAGE_LEN]; // effective message length is variable
    // Format our time-date message
    strftime(message, sizeof (message), "Super-Shell at %r on %B %d, %Y.\n", localtime(&now));
    if (sig == SIGINT) { // ctrl+c -> TERMINATE
        write(STDOUT_FILENO, "\nTerminating ", 13);
        write(STDOUT_FILENO, message, strlen(message));
        exit(EXIT_FAILURE);
    }// Type 'fg' to continue in ANOTHER shell (i.e. BASH). Doesn't work in NetBeans.
    else if (sig == SIGTSTP) { // ctrl+z -> SUSPEND. 
        write(STDOUT_FILENO, "\nSuspending ", 12);
        write(STDOUT_FILENO, message, strlen(message));
        kill(getpid(), SIGSTOP); // suspending
        prompt();
    }
}

/**
 * Overrides standard output.
 * Will redirect output to the specified file name. If the file name
 * given does not exist it will be created. 
 */
void redirect_output_name(char* file) {
    int file_id = open(file, O_CREAT | O_WRONLY | O_TRUNC, 0666);
    redirect_output_id(file_id);
}

/**
 * Overrides standard output.
 * Will redirect output to the specified file_id.
 */
void redirect_output_id(int file_id) {
    close(1); // close standard output
    dup(file_id); // duplicate the file descriptor into slot 1 
    close(file_id);
}

/**
 * Overrides standard input.
 * Will redirect input to be from the file specified by the given file name.
 */
void redirect_input_name(char* file) {
    int file_id = open(file, O_RDONLY, 0666);
    redirect_input_id(file_id);
}

/**
 * Overrides standard input.
 * Will redirect input to be from the file specified by the given file id.
 */
void redirect_input_id(int file_id) {
    close(0); // close standard input
    dup(file_id); // duplicate the file descriptor into slot 0 
    close(file_id);
}

/**
 * Iterates through the first arg_count elements of args, comparing each to s.
 * Returns the first index where there is a match.
 * Returns -1 if no match is found.
 */
int index_of_str(char** args, int arg_count, char* s) {
    int i;
    for (i = 0; i < arg_count; i++) {
        if (str_equals(args[i], s)) {
            return i;
        }
    }
    return -1;
}
