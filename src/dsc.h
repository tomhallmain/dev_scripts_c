#include <stdbool.h>
#include <stdio.h>

#ifndef DSC_H
#define DSC_H

#define BUF_SIZE 1024

//#define DEBUG 0 // comment out to stop debug
#ifdef DEBUG
#define DEBUG_PRINT(x) printf x
#else
#define DEBUG_PRINT(x)                                                         \
  do {                                                                         \
  } while (0)
#endif

struct data_file {
  FILE *fp;
  char filename[64];
  char *fs;
  int fd;
};

typedef struct data_file data_file;

#define FILE_SET data_file[24]

#define PROGRAM_INDEX "index"
#define PROGRAM_IFS "inferfs"
#define PROGRAM_REORDER "reorder"
#define PROGRAM_FIT "fit"

#define SPACE " "
#define TAB "\t"
#define PIPE "|"
#define SEMICOLON ";"
#define COLON ":"
#define COMMA ","
#define SPACEMULTI "[[:space:]]+"
#define SPACEMULTI_ "[[:space:]]{2,}"
#define BACKSLASH "\\"
#define SINGLEQUOT "'"
#define DOUBLEQUOT "\""

#define fail(msg)                                                              \
  fprintf(stderr, msg);                                                        \
  exit(EXIT_FAILURE);

char *intToChar(int x);
int rematch(char *pattern, char *testString);
bool checkStdin(void);
int stdincpy(void);
int getLinesCount(FILE *fp);
int getIntCharLen(int input);
char *nstpcpy(char *buff, char *strings[], int len);

int runIndex(int argc, char **argv, data_file *file);
int inferFieldSeparator(int argc, char **argv, data_file *file);

#endif /* DSC_H */
