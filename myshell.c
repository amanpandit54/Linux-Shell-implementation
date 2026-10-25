/********************************
This is a template for assignment on writing a custom Shell.

Students may change the return types and arguments of the functions given in this template,
but do not change the names of these functions.

Though use of any extra functions is not recommended, students may use new functions if they need to,
but that should not make code unnecessorily complex to read.

Students should keep names of declared variable (and any new functions) self explanatory,
and add proper comments for every logical step.

Students need to be careful while forking a new process (no unnecessory process creations)
or while inserting the single handler code (should be added at the correct places).

Finally, keep your filename as myshell.c, do not change this name (not even myshell.cpp,
as you not need to use any features for this assignment that are supported by C++ but not by C).
*******************************/

#include <stdio.h>
#include <string.h>
#include <stdlib.h> // exit()
#include <unistd.h> // fork(), getpid(), exec()
// #include <sys/wait.h>		// wait()
#include <signal.h> // signal()
#include <fcntl.h>  // close(), open()
#define _POSIX_C_SOURCE 200809L

// Handle Ctrl + Z (SIGTSTP)
void parseInput(char *str)
{
    
     char *pipeToken[10]; // Array to store tokens for pipeline
    int i = 0;
    char *m;
    char *token[10];
    
    // Check for pipeline ('|') and tokenize the input accordingly
    if ((m = strstr(str, "|")) != NULL)
    {
        while ((pipeToken[i] = strsep(&str, "|")) != NULL)
        {
            i++;
            str = (str != NULL) ? (str + 1) : NULL;
        }
        executePipelineCommands(pipeToken, i);
    }
    // Initialize a pointer to the input string
     // Array of strings to store every input on a single command line

    // Use strsep to tokenize the input string
    
    else if ((m = strstr(str, "&&")) != NULL)
    {
        while ((token[i] = strsep(&str, "&")) != NULL)
        {
            i++;
            str = (str != NULL) ? (str + 1) : NULL;
        }
        executeParallelCommands(token, i);
    }
    else if ((m = strstr(str, "##")) != NULL)
    {
        while ((token[i] = strsep(&str, "#")) != NULL)
        {
            i++;
            str = (str != NULL) ? (str + 1) : NULL;
            // if (str[0] == ' ')
            // {
            //     str = str + 1;
            // }
        }
        executeSequentialCommands(token, i);
    }
    else if ((m = strstr(str, ">")) != NULL)
    {
        while ((token[i] = strsep(&str, ">")) != NULL)
        {
            i++;
        }
        executeCommandRedirection(token);
    }
    else
    {
        executeCommand(str); // This takes only string like 'cd aman' so no need of array of strings...
    }
    // after while loop, i gives no. of tokens in the input
}

void executeCommand(char *cmd_array) // this function takes array of strings and no. of token like $cd aman
{
    char *myargs[10];
    if (cmd_array[0] == ' ')
        cmd_array = cmd_array + 1;
    int l = strlen(cmd_array);
    if (cmd_array[l - 1] == ' ')
    {
        cmd_array[l - 1] = '\0';
    }
    int i = 0;
    while ((myargs[i] = strsep(&cmd_array, " ")) != NULL)
    {
        i++;
    }
    // This function will fork a new process to execute a command
    if (strcmp(myargs[0], "exit") == 0)
    {
        printf("Exiting shell...\n");
        // free(cmd);
        exit(EXIT_SUCCESS);
    }
    else if ((strcmp(myargs[0], "cd") == 0) || (strcmp(myargs[0], "cd..") == 0))
    {
        if (strcmp(myargs[0], "cd") == 0)
        {
            int h = chdir(myargs[1]);
            if (h != 0)
            {
                perror("chdir");
                // exit(EXIT_FAILURE);
            }
        }
        else if (strcmp(myargs[0], "cd..") == 0)
        {
            int h = chdir("..");
            if (h != 0)
            {
                perror("chdir");
                // exit(EXIT_FAILURE);
            }
        }
    }

    else
    {
        int pid = fork();
        if (pid == -1)
        {
            perror("Fork not executed...");
        }
        else if (pid == 0)
        {
            if (execvp(myargs[0], myargs) == -1)
            {
                printf("Shell: Incorrect command\n");
                exit(EXIT_SUCCESS);
            }
        }
        else if (pid > 0)
        {
            int wc = wait(NULL);
        }
    }
}

void executeParallelCommands(char **a, int m)
{
    // This function will run multiple commands in parallel
    int child1_pid=fork();
    if (child1_pid<0)
    {
        printf("First Fork failed\n");
    }
    else if (child1_pid==0)
    {
        executeCommand(a[0]);
        exit(NULL);
    }
    else
    {
        int child2_pid=fork();
        if (child2_pid<0)
        {
            printf("Second Fork failed\n");
        }
        else if (child2_pid==0)
        {
            executeCommand(a[1]);
            exit(NULL);
        }
        else{
            wait();
        }
        wait();
    }
}

void executeSequentialCommands(char **a, int m)
{
    // This function will run multiple commands in sequence
    for (int j = 0; j < m; j++)
    {
        executeCommand(a[j]);
    }
}

void executeCommandRedirection(char **b)
{
    // This function will run a single command with output redirected to an output file specificed by user
    char *text_file = malloc(strlen(b[1]) + 1);
    strcpy(text_file, b[1]);
    if (text_file == NULL)
    {
        perror("malloc");
        exit(EXIT_FAILURE);
    }
    if (text_file[0] == ' ')
    {
        text_file = text_file + 1;
    }
    int l = strlen(b[0]);
    if (b[0][l - 1] == ' ')
    {
        b[0][l - 1] = '\0';
    }
    char *f[9];
    int i = 0;
    while ((f[i] = strsep(&b[0], " ")) != NULL)
    {
        i++;
    }
    int h = fork();
    if (h == -1)
    {
        perror("Fork not executed...");
    }
    else if (h == 0)
    {
        close(STDOUT_FILENO);
        open(text_file, O_CREAT | O_WRONLY | O_TRUNC, S_IRWXU);
        if (execvp(f[0], f) == -1)
        {
            printf("Shell: Incorrect command\n");
            exit(EXIT_SUCCESS);
        }
    }
    else if (h > 0)
    {
        int wc = wait(NULL);
    }
}

void executePipelineCommands(char **commands, int numCommands)
{
    if (numCommands < 2)
    {
        // You need at least two commands for a pipeline
        printf("Error: Insufficient commands for a pipeline\n");
        return;
    }

    int numPipes = numCommands - 1;
    int pipes[numPipes][2];

    // Create pipes for communication between commands
    for (int i = 0; i < numPipes; i++)
    {
        if (pipe(pipes[i]) == -1) //pipe() is called to create pipe
        {
            perror("pipe");
            exit(EXIT_FAILURE);
        }
    }

    // Fork and execute each command in the pipeline
    for (int i = 0; i < numCommands; i++)
    {
        int pid = fork();
        if (pid == -1)
        {
            perror("fork");
            exit(EXIT_FAILURE);
        }
        else if (pid == 0) // Child process
        {
            if (i > 0)
            {
                // Redirect input from the previous pipe
                if (dup2(pipes[i - 1][0], STDIN_FILENO) == -1)
                {
                    perror("dup2");
                    exit(EXIT_FAILURE);
                }
            }
            if (i < numPipes)
            {
                // Redirect output to the next pipe
                if (dup2(pipes[i][1], STDOUT_FILENO) == -1)
                {
                    perror("dup2");
                    exit(EXIT_FAILURE);
                }
            }

            // Close all pipe file descriptors in the child
            for (int j = 0; j < numPipes; j++)
            {
                close(pipes[j][0]);
                close(pipes[j][1]);
            }

            // Execute the command
            char *cmdArgs[10];
            int cmdArgCount = 0;
            char *token = strtok(commands[i], " ");
            while (token != NULL)
            {
                cmdArgs[cmdArgCount++] = token;
                token = strtok(NULL, " ");
            }
            cmdArgs[cmdArgCount] = NULL;

            if (execvp(cmdArgs[0], cmdArgs) == -1)
            {
                perror("execvp");
                exit(EXIT_FAILURE);
            }
        }
        else // Parent process
        {
            // Close pipe file descriptors in the parent
            if (i > 0)
            {
                close(pipes[i - 1][0]);
                close(pipes[i - 1][1]);
            }

            // Wait for the child process to complete
            int status;
            if (wait(&status) == -1)
            {
                perror("wait");
                exit(EXIT_FAILURE);
            }
        }
    }
}

void sigtstpHandler(int signum)
{
    // Handle Ctrl + Z (SIGTSTP)
    printf("\n");
}

void sigintHandler(int signum)
{
    // Handle Ctrl + C (SIGINT)
    printf("\n");
}

int main(void)
{
    signal(SIGINT, sigintHandler);   // Handle Ctrl + C (SIGINT)
    signal(SIGTSTP, sigtstpHandler); // Handle Ctrl + Z (SIGTSTP)
    // Initial declarations
    while (1) // This loop will keep your shell running until user exits.
    {
        // Print the prompt in format - currentWorkingDirectory$
        char cwd[1024];
        if (getcwd(cwd, sizeof(cwd)) != NULL)
        {
            printf("%s$", cwd);
        }
        else
        {
            perror("getcwd");
            exit(EXIT_FAILURE);
        }
        // accept input with 'getline()'
        char *line = NULL; // Pointer to store the line
        size_t len = 0;
        ssize_t read;
        if ((read = getline(&line, &len, stdin)) == -1)
        {
            perror("getline");
            free(line);
            exit(EXIT_FAILURE);
        }

        // Remove the newline character at the end (if it exists)
        if (read > 0 && line[read - 1] == '\n')
        {
            line[read - 1] = '\0';
        }

        if (strlen(line) == 0)
        {
            continue;
        }

        // Parse input with 'strsep()' for different symbols (&&, ##, >) and for spaces.
        parseInput(line);

        //		if(/condition/)
        //			executeParallelCommands();		// This function is invoked when user wants to run multiple commands in parallel (commands separated by &&)
        //		else if(/condition/)
        //			executeSequentialCommands();	// This function is invoked when user wants to run multiple commands sequentially (commands separated by ##)
        //		else if(/condition/)
        //			executeCommandRedirection();	// This function is invoked when user wants redirect output of a single command to and output file specificed by user
        //		else
        //			executeCommand();		// This function is invoked when user wants to run a single commands
        free(line);
    }
    return 0;
}