#define _POSIX_C_SOURCE 200809L

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <signal.h>
#include <fcntl.h>
#include <sys/wait.h>
#include <errno.h>

#define MAX_COMMANDS 10
#define MAX_ARGS 10

void parseInput(char *str);
void executeCommand(char *cmd_array);
void executeParallelCommands(char **a, int m);
void executeSequentialCommands(char **a, int m);
void executeCommandRedirection(char **b);
void executePipelineCommands(char **commands, int numCommands);
void sigtstpHandler(int signum);
void sigintHandler(int signum);

/* Remove leading and trailing whitespace from a command. */
static char *trimWhitespace(char *str)
{
    while (*str == ' ' || *str == '\t')
        str++;

    size_t len = strlen(str);
    while (len > 0 && (str[len - 1] == ' ' || str[len - 1] == '\t'))
        str[--len] = '\0';

    return str;
}

/* Split a line using a multi-character operator such as && or ##. */
static int splitCommands(char *str, const char *op, char **commands)
{
    int count = 0;
    size_t opLen = strlen(op);
    char *start = str;
    char *pos;

    while ((pos = strstr(start, op)) != NULL)
    {
        *pos = '\0';
        if (count >= MAX_COMMANDS)
            return -1;

        commands[count++] = trimWhitespace(start);
        start = pos + opLen;
    }

    if (count >= MAX_COMMANDS)
        return -1;

    commands[count++] = trimWhitespace(start);
    return count;
}

/* Execute a command in the current child process. */
static void executeChild(char *command)
{
    char *args[MAX_ARGS + 1];
    char *saveptr = NULL;
    int i = 0;

    command = trimWhitespace(command);
    if (*command == '\0')
        _exit(EXIT_SUCCESS);

    char *token = strtok_r(command, " \t", &saveptr);
    while (token != NULL && i < MAX_ARGS)
    {
        args[i++] = token;
        token = strtok_r(NULL, " \t", &saveptr);
    }
    args[i] = NULL;

    if (i == 0)
        _exit(EXIT_SUCCESS);

    execvp(args[0], args);
    perror("execvp");
    _exit(EXIT_FAILURE);
}

void parseInput(char *str)
{
    char *commands[MAX_COMMANDS];
    char *operatorPos;
    int count;

    str = trimWhitespace(str);
    if (*str == '\0')
        return;

    /* Check operators before executing a normal command. */
    if (strstr(str, "|") != NULL)
    {
        count = splitCommands(str, "|", commands);
        if (count < 2 || count == -1)
        {
            fprintf(stderr, "Invalid pipeline\n");
            return;
        }
        executePipelineCommands(commands, count);
    }
    else if ((operatorPos = strstr(str, "&&")) != NULL)
    {
        count = splitCommands(str, "&&", commands);
        if (count < 2 || count == -1)
        {
            fprintf(stderr, "Invalid parallel command\n");
            return;
        }
        executeParallelCommands(commands, count);
    }
    else if (strstr(str, "##") != NULL)
    {
        count = splitCommands(str, "##", commands);
        if (count < 2 || count == -1)
        {
            fprintf(stderr, "Invalid sequential command\n");
            return;
        }
        executeSequentialCommands(commands, count);
    }
    else if (strstr(str, ">") != NULL)
    {
        count = splitCommands(str, ">", commands);
        if (count != 2 || *commands[0] == '\0' || *commands[1] == '\0')
        {
            fprintf(stderr, "Usage: command > output_file\n");
            return;
        }
        executeCommandRedirection(commands);
    }
    else
    {
        executeCommand(str);
    }
}

void executeCommand(char *cmd_array)
{
    char *command = trimWhitespace(cmd_array);

    if (*command == '\0')
        return;

    if (strcmp(command, "exit") == 0)
    {
        printf("Exiting shell...\n");
        exit(EXIT_SUCCESS);
    }

    /* cd must run in the shell process to change its working directory. */
    if (strncmp(command, "cd", 2) == 0 &&
        (command[2] == '\0' || command[2] == ' ' || command[2] == '\t'))
    {
        char *path = trimWhitespace(command + 2);

        if (*path == '\0')
            path = getenv("HOME");

        if (path == NULL || chdir(path) == -1)
            perror("cd");
        return;
    }

    if (strcmp(command, "cd..") == 0)
    {
        if (chdir("..") == -1)
            perror("cd..");
        return;
    }

    pid_t pid = fork();
    if (pid == -1)
    {
        perror("fork");
    }
    else if (pid == 0)
    {
        executeChild(command);
    }
    else
    {
        while (waitpid(pid, NULL, 0) == -1 && errno == EINTR)
            ;
    }
}

void executeParallelCommands(char **a, int m)
{
    pid_t pids[MAX_COMMANDS];
    int started = 0;

    for (int i = 0; i < m; i++)
    {
        if (*a[i] == '\0')
            continue;

        pid_t pid = fork();
        if (pid == -1)
        {
            perror("fork");
            break;
        }
        if (pid == 0)
            executeChild(a[i]);

        pids[started++] = pid;
    }

    /* Wait only after all commands have been started. */
    for (int i = 0; i < started; i++)
    {
        while (waitpid(pids[i], NULL, 0) == -1 && errno == EINTR)
            ;
    }
}

void executeSequentialCommands(char **a, int m)
{
    for (int i = 0; i < m; i++)
    {
        if (*a[i] != '\0')
            executeCommand(a[i]);
    }
}

void executeCommandRedirection(char **b)
{
    char *command = trimWhitespace(b[0]);
    char *filename = trimWhitespace(b[1]);

    if (*command == '\0' || *filename == '\0')
    {
        fprintf(stderr, "Usage: command > output_file\n");
        return;
    }

    /* A filename containing spaces is not supported by this simple parser. */
    char *saveptr = NULL;
    char *fileToken = strtok_r(filename, " \t", &saveptr);
    if (fileToken == NULL)
    {
        fprintf(stderr, "Missing output filename\n");
        return;
    }

    pid_t pid = fork();
    if (pid == -1)
    {
        perror("fork");
    }
    else if (pid == 0)
    {
        int fd = open(fileToken, O_CREAT | O_WRONLY | O_TRUNC, 0644);
        if (fd == -1)
        {
            perror("open");
            _exit(EXIT_FAILURE);
        }

        if (dup2(fd, STDOUT_FILENO) == -1)
        {
            perror("dup2");
            close(fd);
            _exit(EXIT_FAILURE);
        }
        close(fd);
        executeChild(command);
    }
    else
    {
        while (waitpid(pid, NULL, 0) == -1 && errno == EINTR)
            ;
    }
}

void executePipelineCommands(char **commands, int numCommands)
{
    if (numCommands < 2 || numCommands > MAX_COMMANDS)
    {
        fprintf(stderr, "Invalid number of pipeline commands\n");
        return;
    }

    int pipes[MAX_COMMANDS - 1][2];
    pid_t pids[MAX_COMMANDS];
    int started = 0;

    /* Create all pipes before forking. */
    for (int i = 0; i < numCommands - 1; i++)
    {
        if (pipe(pipes[i]) == -1)
        {
            perror("pipe");
            for (int j = 0; j < i; j++)
            {
                close(pipes[j][0]);
                close(pipes[j][1]);
            }
            return;
        }
    }

    /* Start every command before waiting, so large output cannot deadlock. */
    for (int i = 0; i < numCommands; i++)
    {
        if (*commands[i] == '\0')
        {
            fprintf(stderr, "Empty command in pipeline\n");
            break;
        }

        pid_t pid = fork();
        if (pid == -1)
        {
            perror("fork");
            break;
        }

        if (pid == 0)
        {
            if (i > 0 && dup2(pipes[i - 1][0], STDIN_FILENO) == -1)
            {
                perror("dup2 input");
                _exit(EXIT_FAILURE);
            }

            if (i < numCommands - 1 &&
                dup2(pipes[i][1], STDOUT_FILENO) == -1)
            {
                perror("dup2 output");
                _exit(EXIT_FAILURE);
            }

            for (int j = 0; j < numCommands - 1; j++)
            {
                close(pipes[j][0]);
                close(pipes[j][1]);
            }

            executeChild(commands[i]);
        }

        pids[started++] = pid;
    }

    /* Parent must close every pipe end before waiting. */
    for (int i = 0; i < numCommands - 1; i++)
    {
        close(pipes[i][0]);
        close(pipes[i][1]);
    }

    for (int i = 0; i < started; i++)
    {
        while (waitpid(pids[i], NULL, 0) == -1 && errno == EINTR)
            ;
    }
}

void sigtstpHandler(int signum)
{
    (void)signum;
    write(STDOUT_FILENO, "\n", 1);
}

void sigintHandler(int signum)
{
    (void)signum;
    write(STDOUT_FILENO, "\n", 1);
}

int main(void)
{
    signal(SIGINT, sigintHandler);
    signal(SIGTSTP, sigtstpHandler);

    while (1)
    {
        char cwd[1024];

        if (getcwd(cwd, sizeof(cwd)) != NULL)
            printf("%s$", cwd);
        else
        {
            perror("getcwd");
            exit(EXIT_FAILURE);
        }

        fflush(stdout);

        char *line = NULL;
        size_t len = 0;
        ssize_t readCount = getline(&line, &len, stdin);

        if (readCount == -1)
        {
            free(line);
            if (feof(stdin))
            {
                printf("\n");
                break;
            }
            if (errno == EINTR)
            {
                clearerr(stdin);
                continue;
            }
            perror("getline");
            break;
        }

        if (readCount > 0 && line[readCount - 1] == '\n')
            line[readCount - 1] = '\0';

        if (*trimWhitespace(line) != '\0')
            parseInput(line);

        free(line);
    }

    return 0;
}
