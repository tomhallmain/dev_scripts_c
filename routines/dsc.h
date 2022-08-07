#include <stdbool.h>
#include <stdio.h>

#ifndef DSC_H
#define DSC_H

#define BUF_SIZE 1024
#define FILE_SET FILE *[24]

#define DEBUG 0 // comment out to stop debug
#ifdef DEBUG
#define DEBUG_PRINT(x) printf x
#else
#define DEBUG_PRINT(x)                                                         \
  do {                                                                         \
  } while (0)
#endif

#define PROGRAM_INDEX "index"
#define PROGRAM_IFS "inferfs"
#define PROGRAM_REORDER "reorder"
#define PROGRAM_FIT "fit"

#define match(x, y) strcmp(x, y) == 0
#define fail(msg)                                                              \
  fprintf(stderr, msg);                                                        \
  exit(EXIT_FAILURE);

char *intToChar(int x);
int regexMatch(char *pattern, char *testString);
bool checkStdin(void);
int stdincpy(void);
int getLinesCount(FILE *fp);
int getIntCharLen(int input);

int runIndex(int argc, char **argv, char *filename, int fd, char *fs);
char *inferFieldSeparator(int argc, char **argv, char *filename, int fd);

#endif /* DSC_H */
