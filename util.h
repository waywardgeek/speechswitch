#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>

// These utilities are provided simply to aid portability.
char **swListDirectory(const char *dirName, uint32_t *numFiles);
// Determine if the file exists and is readable.
bool swFileReadable(const char *fileName);
// Make a copy of the string.  The caller is responsible for calling free.
char *swCopyString(const char *string);
// Concatenate two strings.  The caller is responsible for calling free.
char *swCatStrings(const char *string1, const char *string2);
// This function frees a voice list created with getVoices.
void swFreeStringList(char **stringList, uint32_t numStrings);
char **swCopyStringList(const char **stringList, uint32_t numStrings);
// Read up to a newline or EOF.  Do not include the newline character.
// The result must be freed by the caller.
char *swReadLine(FILE *file);

// Create a child process and return two FILE objects for communication.  The
// child process simply uses stdin/stdout for communication.  The arguments to
// the child process should be passed as additional parameters, ending with a
// NULL.  Return the child PID.
int swForkWithStdio(const char *exePath, FILE **fin, FILE **fout, ...);

// Convert an ANSI character to ASCII.  The returned string is zero-terminated.
// This returns a static buffer and is not thread safe.
char *swConvertANSIToASCII(char c);
// The returned word will be at most this long
#define SW_MAX_WORD_SIZE 16
