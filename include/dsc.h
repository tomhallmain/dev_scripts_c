#ifndef DSC_H
#define DSC_H

#include <ctype.h>
#include <regex.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../lib/gnu_utils.h"
#include "hashmap.h"
#include "posix_compat.h"

// Error handling
#define DSC_SUCCESS 0
#define DSC_ERROR -1

// Debug macros
#ifdef NDEBUG
    #define DEBUG_PRINT(...) ((void)0)
    #define IS_DEBUG 0
#else
    #ifdef __GNUC__
        // GCC-specific format checking
        #define DEBUG_PRINT_IMPL(fmt, ...) do { \
            fprintf(stderr, "DEBUG: "); \
            fprintf(stderr, fmt, ##__VA_ARGS__); \
            fprintf(stderr, "\n"); \
        } while(0)
        #define DEBUG_PRINT(fmt, ...) do { \
            if (__builtin_constant_p(fmt)) { \
                /* Compile-time format string check */ \
                extern int fprintf(FILE *stream, const char *format, ...) \
                    __attribute__((format(printf, 2, 3))); \
                /* Just check the format, don't print */ \
                (void)(sizeof(fprintf(stderr, fmt, ##__VA_ARGS__))); \
            } \
            DEBUG_PRINT_IMPL(fmt, ##__VA_ARGS__); \
        } while(0)
    #else
        // Non-GCC compilers get runtime checking
        #define DEBUG_PRINT_IMPL(fmt, ...) do { \
            fprintf(stderr, "DEBUG: "); \
            fprintf(stderr, fmt, ##__VA_ARGS__); \
            fprintf(stderr, "\n"); \
        } while(0)
        #define DEBUG_PRINT(fmt, ...) do { \
            /* Runtime format string validation */ \
            if (strchr(fmt, '%') != NULL) { \
                int expected_args = 0; \
                const char *p = fmt; \
                while ((p = strchr(p, '%')) != NULL) { \
                    if (*(p + 1) != '%') expected_args++; \
                    p++; \
                } \
                int actual_args = sizeof((int[]){0, ##__VA_ARGS__}) / sizeof(int) - 1; \
                if (expected_args != actual_args) { \
                    fprintf(stderr, "DEBUG_PRINT ERROR: Format string '%s' expects %d arguments but got %d\n", \
                            fmt, expected_args, actual_args); \
                    abort(); \
                } \
            } \
            DEBUG_PRINT_IMPL(fmt, ##__VA_ARGS__); \
        } while(0)
    #endif
    #define IS_DEBUG 1
#endif

// Maximum lengths
#define DSC_MAX_CMD_NAME 32
#define DSC_MAX_FILENAME 256
#define DSC_MAX_LINE_LENGTH 4096
#define BUF_SIZE 1024
#define MAX_NF_COUNT 128

// Command function types
typedef int (*command_main_func)(int argc, char **argv, void *data);
typedef int (*command_fs_func)(int argc, char **argv, void *data);

// Field separator structure
typedef struct field_sep {
    char key;
    char *sep;
    bool is_regex;
    int total;
    double sum_var;
    double var;
    int prev_nf;
} field_sep;

// Field reference structure
typedef struct {
    int start;    /* start index */
    short len;    /* end index */
    short row;    /* row index of this field */
    short col;    /* column index of this field */
} fieldref;

// Command structure
typedef struct {
    const char *name;
    command_main_func main_func;
    command_fs_func fs_func;
    bool use_file;
    bool is_inferfs;
    int expected_filename_index;
    bool terminating;
} command_t;

// Data file structure
typedef struct {
    char filename[DSC_MAX_FILENAME];
    int fd;
    int tmp_file_index;
    bool is_piped;
    bool remove_file_arg;
    field_sep *fs;
} data_file_t;

#define FILE_SET data_file_t[24]

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
#define FAIL(msg) do { \
    fprintf(stderr, "%s\n", msg); \
    exit(EXIT_FAILURE); \
} while(0)

#define UNREACHABLE(reason) do { \
    DEBUG_PRINT("%s", reason); \
    FAIL(reason); \
} while(0)

#define ARRAY_SIZE(x) (sizeof(x) / sizeof((x)[0]))

// Core functions
int dsc_init(void);
void dsc_cleanup(void);
command_t *dsc_get_command(const char *name);
int dsc_parse_command(const char *cmd, int *argc, char ***argv);
void dsc_free_command_args(int argc, char **argv);

// Error handling
void dsc_set_error(const char *fmt, ...);
const char *dsc_get_error(void);

// File handling
int dsc_open_file(data_file_t *file, const char *filename);
void dsc_close_file(data_file_t *file);
int dsc_clear_temp_files(void);
FILE *get_readable_fp(data_file_t *file);

// Utility functions
void nstpcpy(char *buff, char *strings[], int len);
char *int_to_char(int x);
int get_lines_count(FILE *fp);
int get_int_char_len(int input);
char *substr(const char *src, int m, int n);
int endswith(const char *str, const char *suffix);
regex_t* get_compiled_regex(char *pattern, bool reuse);
int rematch(char *pattern, char *test_string, bool reuse);
int count_matches_for_line_char(const char sep_char, const char *line, size_t len);
int count_matches_for_line_str(const char *sep, char *line, size_t len);
int count_matches_for_line_regex(const char *sep, const char *line, size_t len);
void hex_dump(char *desc, void *addr, int len);
void bucket_dump_regex(void);

// Field handling
void find_fields_in_text(BLOCK text_buffer, char *fs, int is_regex,
                        int total_line_count[1], int max_field_length[1],
                        fieldref *fields_table[1], int number_of_fields[1]);
void debug_fields_table(fieldref *fields_table, char *file_buffer,
                       int total_line_count, int max_column_count,
                       int number_of_fields);
fieldref *get_ref_for_field(fieldref *fields_table, int number_of_fields,
                           int row, int col);

// Command implementations
int run_index(int argc, char **argv, data_file_t *file);
int infer_field_separator(int argc, char **argv, data_file_t *file);
int get_random(int argc, char **argv, data_file_t *file);
int transpose(int argc, char **argv, data_file_t *file);
int fit(int argc, char **argv, data_file_t *file);
int reorder(int argc, char **argv, data_file_t *file);

#endif // DSC_H 