#include "dsc.h"
#include "map.h"
#include "hashmap.h"
#include <regex.h>
#include <stdlib.h>
#include <string.h>

#define MAX_TEMP_FILES 20

static int tmp_file_index = 0;
typedef struct tmp_file {
    char *filename;
    int fd;
} tmp_file;
static tmp_file tmp_files[MAX_TEMP_FILES];

static hashmap *regex_map = NULL;

// Cleanup function for regex patterns
static void cleanup_regex(void* key, void* value) {
    regex_t* regex = (regex_t*)value;
    regfree(regex);
    free(regex);
}

// Common utilities methods

static int stdincpy(data_file *file) {
    if (file->tmp_file_index > -1) {
        UNREACHABLE("base - stdin should already be copied to a temporary file");
    } else if (tmp_file_index == MAX_TEMP_FILES - 1) {
        FAIL("base - Too many temporary files");
    }

    char template[21] = "/tmp/ds_stdin_XXXXXX";
    tmp_file temp;
    temp.fd = mkstemp(template);
    char *filename = malloc(sizeof(char) * (strlen(template) + 1));
    stpcpy(filename, template);
    temp.filename = filename;
    file->tmp_file_index = tmp_file_index;
    tmp_files[tmp_file_index++] = temp;

    DEBUG_PRINT(("base - using stdin, copying to temporary file\n"));
    DEBUG_PRINT(("base - %s\n", tmp_files[file->tmp_file_index].filename));

    if (feof(stdin))
        printf("base - stdin reached eof\n");

    FILE *fp = fdopen(temp.fd, "w");

    if (fp == 0)
        printf("base - something went wrong opening temp file\n");

    unsigned long nbytes;
    char buffer[BUF_SIZE];

    while ((nbytes = fread(buffer, 1, BUF_SIZE, stdin))) {
        fwrite(buffer, nbytes, 1, fp);
    }

    if (ferror(stdin))
        printf("base - error reading from stdin");

    rewind(fp);
    return temp.fd;
}

FILE *get_readable_fp(data_file *file) {
    if (file->is_piped) {
        if (file->tmp_file_index < 0) {
            int fd = stdincpy(file);
            return fdopen(fd, "r");
        } else {
            return fdopen(tmp_files[file->tmp_file_index].fd, "r");
        }
    } else if (file->fd > 2) {
        return fopen(file->filename, "r");
    } else {
        printf("base - Filename: %s fd: %d \n", file->filename, file->fd);
        FAIL("base - Failed to get readable file pointer.");
    }
}

int clear_temp_files(void) {
    int result = 0;
    for (int i = tmp_file_index - 1; i >= 0; i--) {
        if (remove(tmp_files[i].filename)) {
            result = -1;
        } else if (result > -1) {
            result = 1;
        }
        free(tmp_files[i].filename);
        tmp_files[i].fd = -1;
    }
    return result;
}

// if (checkStdin()) {
//     fp = tmpfile();
//     char ch;
//     while((ch = fgetc(stdin)) != EOF) {
//         fputc(ch, fp);
//     }
//     rewind(fp);
// }
// static bool check_stdin(void) {
//   DEBUG_PRINT(("%s\n", "in checkStdin"));
//   int ch = getchar();
//   if (ch == EOF) {
//     DEBUG_PRINT(("%s\n", "char was EOF"));
//     if (!(feof(stdin))) {
//       puts("stdin error"); // very rare
//     }
//     return false;
//   } else {
//     DEBUG_PRINT(("%s\n", "char was not EOF"));
//     ungetc(ch, stdin); // put back
//     DEBUG_PRINT(("%s\n", "char put back"));
//     return true;
//   }
// }

void nstpcpy(char *buff, char *strings[], int len) {
    for (int i = 0; i < len; i++) {
        buff = stpcpy(buff, strings[i]);
    }
}

int get_lines_count(FILE *fp) {
    int totalLines = 0;

    if (fp == NULL) {
        DEBUG_PRINT(("%s\n", "fp was NULL"));
        FAIL("Error while opening the file.");
    }

    int chr = getc(fp);
    while (chr != EOF) {
        if (chr == '\n') {
            totalLines++;
        }
        chr = getc(fp);
    }

    if (fileno(fp) > 2) {
        rewind(fp);
    }

    return totalLines;
}

char *int_to_char(int x) {
    char *x_char;

    if (sprintf(x_char, "%d", x) == -1) {
        FAIL("sprintf");
    } else {
        return x_char;
    }
}

int get_int_char_len(int input) {
    int len = 1, test10sPlace = 10, negative = 0;
    if (input < 0) {
        negative = 1;
        test10sPlace *= -1;
        while (input <= test10sPlace) {
            test10sPlace *= 10;
            len++;
        }
        test10sPlace *= -1;
    } else {
        while (input >= test10sPlace) {
            test10sPlace *= 10;
            len++;
        }
    }
    return len + negative;
}

// Following function extracts characters present in `src`
// between `m` and `n` (excluding `n`)
char *substr(const char *src, int m, int n) {
    int len = n - m;
    // allocate (len + 1) chars for destination
    char *dest = (char *)malloc(sizeof(char) * (len + 1));

    // extract characters between m'th and n'th index from source string
    // and copy them into the destination string
    for (int i = m; i < n && (*(src + i) != '\0'); i++) {
        *dest = *(src + i);
        dest++;
    }

    // null-terminate the destination string
    *dest = '\0';
    return dest - len;
}

int endswith(const char *str, const char *suffix) {
    if (!str || !suffix)
        return 0;
    size_t lenstr = strlen(str);
    size_t lensuffix = strlen(suffix);
    if (lensuffix > lenstr)
        return 0;
    return strncmp(str + lenstr - lensuffix, suffix, lensuffix) == 0;
}

void bucket_dump_regex() {
    // This function is no longer needed with the new hashmap implementation
    // but we'll keep it for compatibility
    if (regex_map) {
        printf("Regex cache size: %zu\n", hashmap_size(regex_map));
    }
}

regex_t get_compiled_regex(char *pattern, bool reuse) {
    regex_t regex;

    if (reuse) {
        if (!regex_map) {
            regex_map = hashmap_create(16);
            if (!regex_map) {
                FAIL("base - Failed to create regex cache");
            }
        }

        void* cached_regex;
        if (hashmap_get_string(regex_map, pattern, &cached_regex)) {
            return *(regex_t*)cached_regex;
        } else {
            int compile_err = regcomp(&regex, pattern, REG_EXTENDED);
            if (compile_err) {
                DEBUG_PRINT(("base - Failed to compile regex for pattern \"%s\" with error %d",
                            pattern, compile_err));
                FAIL("base - rematch error - could not compile regex");
            }

            // Store the compiled regex
            regex_t* stored_regex = malloc(sizeof(regex_t));
            if (!stored_regex) {
                regfree(&regex);
                FAIL("base - Failed to allocate memory for regex");
            }
            *stored_regex = regex;

            if (!hashmap_put_string(regex_map, pattern, stored_regex)) {
                regfree(&regex);
                free(stored_regex);
                FAIL("base - Failed to store regex in cache");
            }
        }
    } else {
        int compile_err = regcomp(&regex, pattern, REG_EXTENDED);
        if (compile_err) {
            DEBUG_PRINT(("base - Failed to compile regex for pattern \"%s\" with error %d",
                        pattern, compile_err));
            FAIL("base - rematch error - could not compile regex");
        }
    }

    return regex;
}

int rematch(char *pattern, char *test_string, bool reuse) {
    regex_t regex = get_compiled_regex(pattern, reuse);
    int result = regexec(&regex, test_string, 0, NULL, 0);
    return result ? 0 : 1;
}

regmatch_t *re_search(char *pattern, const char *string, int length, int start,
                      bool reuse) {
    regex_t regex = get_compiled_regex(pattern, reuse);
    regmatch_t pmatch[1];
    char *search_array = malloc(length + 1);
    memcpy(search_array, pattern, length);
    int result = regexec(&regex, search_array, 0, pmatch, 0);
    return result ? &(pmatch[0]) : NULL;
}

char *fixed_search(const char *buffer, const char *string) {
    // Find the length of the search string
    size_t string_length = strlen(string);

    // Loop through the buffer until we find the search string or reach the end
    const char *p = buffer;
    while (*p != '\0') {
        // Check if the current position in the buffer starts with the search string
        if (strncmp(p, string, string_length) == 0) {
            // If it does, return a pointer to the current position in the buffer
            //      DEBUG_PRINT(("Found character: %c", *p));
            return (char *)p;
        }
        p++;
    }

    // If we reach here, we did not find the search string in the buffer
    return NULL;
}

// Simple character count
int count_matches_for_line_char(const char sep_char, char *line, size_t len) {
    int count = 1;

    for (int i = 0; i < len; i++) {
        if (line[i] == sep_char) {
            ++count;
        }
    }

    return count;
}

int count_matches_for_line_str(const char *sep, char *line, size_t len) {
    int count = 0;
    size_t sep_len = strlen(sep);
    bool is_char = sep_len == 1;

    if (is_char) {
        return count_matches_for_line_char(sep[0], line, len);
    } else {
        // Fixed string count
        for (int i = 0; i < len; i++) {
            bool all_match = true;
            int j;
            for (j = 0; j < sep_len; j++) {
                if (line[i + j] != sep[j]) {
                    all_match = false;
                }
            }
            if (all_match) {
                count++;
                i += j - 1;
            }
        }
    }

    return count;
}

int count_matches_for_line_regex(const char *sep, char *line, size_t len) {
    // Regex count
    int count = 0;
    int position = 0;
    regex_t regex;
    // TODO figure out why get_compiled_regex isn't working here
    regcomp(&regex, sep, REG_EXTENDED);

    while (position < len) {
        regmatch_t pmatch[1];
        int err = regexec(&regex, &line[position], 1, pmatch, 0);
        if (!err) {
            ++count;
            position += (int)pmatch[0].rm_eo;
            if (count > 500) {
                break;
            }
        } else if (err == REG_NOMATCH) {
            break;
        } else {
            printf("base - %d\n", err);
            FAIL("base - Regex match failed.");
        }
    }

    if (count > 0)
        count--;

    return count;
}

void hex_dump(char *desc, void *addr, int len) {
    int i;
    unsigned char buff[17];
    unsigned char *pc = (unsigned char *)addr;

    // Output description if given.
    if (desc != NULL)
        printf("%s:\n", desc);

    // Process every byte in the data.
    for (i = 0; i < len; i++) {
        // Multiple of 16 means new line (with line offset).

        if ((i % 16) == 0) {
            // Just don't print ASCII for the zeroth line.
            if (i != 0)
                printf("  %s\n", buff);

            // Output the offset.
            printf("  %04x ", i);
        }

        // Now the hex code for the specific character.
        printf(" %02x", pc[i]);

        // And store a printable ASCII character for later.
        if ((pc[i] < 0x20) || (pc[i] > 0x7e)) {
            buff[i % 16] = '.';
        } else {
            buff[i % 16] = pc[i];
        }

        buff[(i % 16) + 1] = '\0';
    }

    // Pad out last line if not exactly 16 characters.
    while ((i % 16) != 0) {
        printf("   ");
        i++;
    }

    // And print the final ASCII bit.
    printf("  %s\n", buff);
}

void find_fields_in_text(BLOCK text_buffer, char *fs, int is_regex,
                         int total_line_count[1], int max_field_length[1],
                         fieldref *fields_table[1], int number_of_fields[1]) {
    /* Initialize variables */
    int line_count = 0;
    int max_field_len = 0;
    int field_count = 0;
    int current_row = 0;
    int current_col = 0;
    int fields_table_size = 10;

    /* Allocate memory for the fields table */
    fields_table[0] = malloc(fields_table_size * sizeof(fieldref));

    /* Start parsing the text */
    char *current_position = text_buffer.start;
    char *next_fs_start;
    char *next_fs_end;
    size_t fs_size = strlen(fs);
    regmatch_t *match;

    while (current_position < text_buffer.end) {
        DEBUG_PRINT(("row count = %d\n", current_row));
        DEBUG_PRINT(("col count = %d\n", current_col));
        int end_line = 0;
        char *line_end = index(current_position, '\n');
        fieldref field;

        if (is_regex) {
            match = re_search(fs, current_position,
                              text_buffer.end - current_position, 0, true);
            next_fs_start = match ? current_position + match->rm_so : NULL;
            next_fs_end = match ? current_position + match->rm_eo - 1 : NULL;
        } else {
            next_fs_start = fixed_search(current_position, fs);
            next_fs_end = next_fs_start + fs_size;
        }

        field.start = current_position - text_buffer.start;
        field.row = current_row;
        field.col = current_col;

        if (next_fs_start != NULL) {
            DEBUG_PRINT(("next_fs_start = %d\n", next_fs_start));
            DEBUG_PRINT(("next_fs_end = %d\n", next_fs_end));
            DEBUG_PRINT(("line_end = %d\n", line_end));

            if (line_end == NULL || next_fs_start < line_end) {
                field.len = next_fs_start - current_position;
                current_position = next_fs_end;
            } else {
                field.len = line_end - current_position;
                end_line = 1;
                current_position = line_end + 1;
            }
        } else {
            DEBUG_PRINT(("current col %d had null next_fs_start\n", current_col));
            if (line_end == NULL) {
                field.len = text_buffer.end - current_position;
                current_position = text_buffer.end;
            } else {
                field.len = line_end - current_position;
                current_position = line_end + 1;
            }
            end_line = 1;
        }

    if (field.len > max_field_len) {
        max_field_len = field.len;
    }

    /* Check if the fields table needs to be resized */
    if (field_count == fields_table_size) {
        fields_table_size *= 2;
        fields_table[0] =
            realloc(fields_table[0], fields_table_size * sizeof(fieldref));
    }

    /* Add the new field to the fields table */
    fields_table[0][field_count] = field;
    field_count++;

    if (end_line) {
        line_count++;
        current_row++;
        current_col = 0;
    } else {
        current_col++;
    }
  }

    /* Save the results in the output variables */
    *total_line_count = line_count;
    *max_field_length = max_field_len;
    *number_of_fields = field_count;
}

void debug_fields_table(fieldref *fields_table, char *file_buffer,
                        int total_line_count, int max_column_count,
                        int number_of_fields) {

    int total_len = 0;
    int fs_size = 1;

    for (int i = 0; i < total_line_count; i++) {
        for (int j = 0; j < max_column_count; j++) {
            fieldref *ref = get_ref_for_field(fields_table, number_of_fields, i, j);

            if (ref != NULL) {
                //        printf("Ref found for %d %d: %d %d %d %d\n", i, j, ref->start,
                //        ref->len, ref->row, ref->col);
                total_len += ref->len;
            }
            //      else{
            //        printf("Ref for %d %d not found, maybe empty field.\n", i , j);
            //      }

            if (j < total_line_count - 1) {
                total_len += fs_size;
            }
        }

        if (i < max_column_count - 1) {
            total_len += 1;
        }
    }

    char *buffer = malloc(total_len + 1);
    memset(buffer, ' ', total_len + 1);
    buffer[total_len] = '\0';
    total_len = 0;
    char *fs = ",";
    char *newline = "\n";

    for (int i = 0; i < total_line_count; i++) {
        for (int j = 0; j < max_column_count; j++) {
            fieldref *ref = get_ref_for_field(fields_table, number_of_fields, i, j);

            if (ref != NULL) {
                char *field = file_buffer + ref->start;
                //        printf("%d %d %d %d\n", ref->start, ref->len, ref->row,
                //        ref->col); printf("%s\n", field);
                char *transposed_field = buffer + total_len;
                memcpy(transposed_field, field, ref->len);
                //        printf("%s\n", transposed_field);
                total_len += ref->len;
            }

            if (j < max_column_count - 1) {
                memcpy(buffer + total_len, fs, fs_size);
                total_len += 1;
            }
        }

        if (i < total_line_count - 1) {
            memcpy(buffer + total_len, newline, 1);
            total_len += 1;
        }
    }

    printf("%s", buffer);
}

fieldref *get_ref_for_field(fieldref *fields_table, int number_of_fields,
                            int row, int col) {
    for (int i = 0; i < number_of_fields; i++) {
        if (fields_table[i].row == row && fields_table[i].col == col) {
            return &fields_table[i];
        } else if (fields_table[i].row > row) {
            break;
        }
    }

    return NULL;
}

// Add cleanup function to be called at program exit
void cleanup_base(void) {
    if (regex_map) {
        hashmap_free(regex_map, cleanup_regex);
        regex_map = NULL;
    }
}
