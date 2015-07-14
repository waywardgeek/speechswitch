#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <dirent.h>
#include <stdarg.h>
#include <unistd.h>

#include "util.h"

// Just make a copy of a string.
char *swCopyString(char *string)
{
    char *newString = (char *)calloc(strlen(string) + 1, sizeof(char));

    strcpy(newString, string);
    return newString;
}

// Just concatenate two strings.
char *swCatStrings(char *string1, char *string2)
{
    char *newString = (char *)calloc(strlen(string1) + strlen(string2) + 1, sizeof(char));

    strcpy(newString, string1);
    strcat(newString, string2);
    return newString;
}

// This utility is provided to list directory entries in a portable way.
char **swListDirectory(char *dirName, uint32_t *numFiles)
{
    DIR *dir;
    struct dirent *entry;
    char **fileList;
    int i;
 
    *numFiles = 0;
    dir = opendir(dirName);
    if(dir == NULL) {
        return NULL;
    }
    entry = readdir(dir);
    while(entry != NULL) {
        if(strcmp(entry->d_name, ".") && strcmp(entry->d_name, "..")) {
            (*numFiles)++;
        }
        entry = readdir(dir);
    }
    (void)closedir(dir);
    dir = opendir(dirName);
    if(dir == NULL) {
        return NULL;
    }
    fileList = (char **)calloc(*numFiles, sizeof(char *));
    for(i = 0; i < *numFiles; i++) {
        entry = readdir(dir);
        while(!strcmp(entry->d_name, ".") || !strcmp(entry->d_name, "..")) {
            entry = readdir(dir);
        }
        fileList[i] = swCopyString(entry->d_name);
    }
    (void)closedir(dir);
    return fileList;
}

// This function, frees a voice list created with getVoices.
void swFreeStringList(char **stringList, uint32_t numStrings)
{
    int i;

    for(i = 0; i < numStrings; i++) {
        free(stringList[i]);
    }
    free(stringList);
}

// Make a copy of a string list.
char **swCopyStringList(char **stringList, uint32_t numStrings)
{
    char **newList = (char **)calloc(numStrings, sizeof(char*));
    int i;

    for(i = 0; i < numStrings; i++) {
        newList[i] = swCopyString(stringList[i]);
    }
    return newList;
}

// Read up to a newline or EOF.  Do not include the newline character.
// The result must be freed by the caller.
char *swReadLine(FILE *file) {
    uint32_t bufSize = 42;
    char *buf = calloc(bufSize, sizeof(char));
    uint32_t pos = 0;
    int c = getc(file);
    while(c != EOF && c != '\n') {
        if(pos == bufSize) {
            bufSize <<= 1;
            buf = realloc(buf, bufSize);
        }
        buf[pos++] = c;
        c = getc(file);
    }
    return buf;
}

#define MAXARGS 42

// Create a child process and return two FILE objects for communication.  The
// child process simply uses stdin/stdout for communication.  The arguments to
// the child process should be passed as additional parameters, ending with a
// NULL.
int swForkWithStdio(char *exePath, FILE **fin, FILE **fout, ...) {
    // Build the parameter list
    char *args[MAXARGS];
    va_list ap;
    va_start(ap, fout);
    int i = 0;
    args[i++] = exePath;
    char *param = va_arg(ap, char *);
    while(param != NULL) {
        if(i+1 == MAXARGS) {
            fprintf(stderr, "Too many arguments to swForkWithStdio\n");
            exit(1);
        }
        args[i++] = param;
        param = va_arg(ap, char *);
    }
    va_end(ap);
    args[i] = NULL;

    // Create pips and fork
    int pipes[2][2];
    if(pipe(pipes[0]) != 0 || pipe(pipes[1]) != 0) {
        fprintf(stderr, "Unable to allocate pipes\n");
        exit(1);
    }
    int pid = fork();
    if(pid == 0) {
        // Child process: overwrite stdin and stdout
        close(pipes[0][0]);
        close(pipes[1][1]);
        dup2(pipes[0][1], STDOUT_FILENO);
        dup2(pipes[1][0], STDIN_FILENO);
        // Exec the program
        execv(exePath, args);
    }

    // Parent program.  Create fin/fout and return.
    close(pipes[0][1]);
    close(pipes[1][0]);
    *fin = fdopen(pipes[0][0], "r");
    *fout = fdopen(pipes[1][1], "w");
    return pid;
}