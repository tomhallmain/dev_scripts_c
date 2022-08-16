#include <regex.h>
#include <stdbool.h>
#include <stdio.h>

#ifndef DSC_H
#define DSC_H

#define BUF_SIZE 1024

// #define DEBUG 0 // comment out to stop debug
#ifdef DEBUG
#define DEBUG_PRINT(x) printf x
#define IS_DEBUG true
#else
#define DEBUG_PRINT(x)                                                         \
  do {                                                                         \
  } while (0)
#define IS_DEBUG false
#endif

struct data_file {
  FILE *fp;
  char filename[128];
  char *fs;
  int fd;
  bool is_piped;
};

typedef struct data_file data_file;

#define FILE_SET data_file[24]

#define PROGRAM_INDEX "index"
#define PROGRAM_IFS "inferfs"
#define PROGRAM_RANDOM "random"
#define PROGRAM_REORDER "reorder"
#define PROGRAM_FIT "fit"

#define DS_SEP "@@@"
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

#define FAIL(msg)                                                              \
  fprintf(stderr, "%s\n", msg);                                                \
  exit(EXIT_FAILURE);

#define UNREACHABLE(reason)                                                    \
  DEBUG_PRINT(("%s\n", "Hit unreachable statement"));                          \
  fail(reason);

char *int_to_char(int x);
char *substr(const char *src, int m, int n);
regex_t get_compiled_regex(char *pattern, bool reuse);
int rematch(char *pattern, char *test_string, bool reuse);
bool check_stdin(void);
int stdincpy(void);
int get_lines_count(FILE *fp);
int get_int_char_len(int input);
void nstpcpy(char *buff, char *strings[], int len);

int run_index(int argc, char **argv, data_file *file);
int infer_field_separator(int argc, char **argv, data_file *file);
int get_random(int argc, char **argv, data_file *file);

#endif /* DSC_H */
