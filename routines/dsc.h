#include <stdbool.h>
#include <stdio.h>

#ifndef DSC_H
#define DSC_H

#define MAX_LEN 1024

//#define DEBUG 0 // comment out to stop debug
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

char *intToChar(int x);
bool checkStdin(void);
int getLinesCount(FILE *fp);
int getIntCharLen(int input);

int runIndex(int argc, char **argv, char *filename, char *fs);
int runInferFieldSeparator(int argc, char **argv, char *filename, char *fs);
char *inferFieldSeparator(char *filename);

#endif /* DSC_H */
