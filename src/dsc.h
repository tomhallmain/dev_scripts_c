#include <regex.h>
#include <stdbool.h>
#include <stdio.h>

#ifndef DSC_H
#define DSC_H

#define BUF_SIZE 1024

// Debug

//#define DEBUG 0 // comment out to stop debug
#ifdef DEBUG
#define DEBUG_PRINT(x) printf x
#define IS_DEBUG true
#else
#define DEBUG_PRINT(x)                                                         \
  do {                                                                         \
  } while (0)
#define IS_DEBUG false
#endif

// File management

typedef struct data_file {
  FILE *fp;
  char filename[128];
  char *fs;
  int fd;
  bool is_piped;
  int tmp_file_index;
  bool remove_file_arg;
} data_file;
#define FILE_SET data_file[24]

// Patterns and separators

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

#define EMPTY_LINE_RE "^ *$"

// Macros

#define FAIL(msg)                                                              \
  fprintf(stderr, "%s\n", msg);                                                \
  exit(EXIT_FAILURE);

#define UNREACHABLE(reason)                                                    \
  DEBUG_PRINT(("Hit unreachable statement: %s\n", reason));                    \
  FAIL(reason);

#define ARRAY_SIZE(x) (sizeof(x) / sizeof((x)[0]))

// Functions from base.c (TODO maybe move to new header)

FILE *get_readable_fp(data_file *file);
int clear_temp_files(void);
void nstpcpy(char *buff, char *strings[], int len);
char *int_to_char(int x);
int get_lines_count(FILE *fp);
int get_int_char_len(int input);
char *substr(const char *src, int m, int n);
int endswith(const char *str, const char *suffix);
regex_t get_compiled_regex(char *pattern, bool reuse);
int rematch(char *pattern, char *test_string, bool reuse);

// Program functions

int run_index(int argc, char **argv, data_file *file);
int infer_field_separator(int argc, char **argv, data_file *file);
int get_random(int argc, char **argv, data_file *file);
int transpose(int argc, char **argv, data_file *file);
int fit(int argc, char **argv, data_file *file);
int reorder(int argc, char **argv, data_file *file);

#endif /* DSC_H */
