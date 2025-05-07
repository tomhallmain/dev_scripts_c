#include "../include/dsc.h"
#include <regex.h>
#include <stdlib.h>
#include <string.h>

// Transpose the field values of a text-based field-separated file.
int transpose(int argc, char **argv, data_file_t *file) {
    DEBUG_PRINT(("transpose - Running transpose\n"));
    char fs[15];
    char ofs[15];
    strcpy(fs, file->fs->sep);

    // adjust field separator for special cases
    if (rematch("\\[\\[:space:\\]\\]\\{2.\\}", fs, false)) {
        strcpy(ofs, "  ");
    } else if (rematch("\\[.+\\]", fs, false)) {
        strcpy(ofs, " ");
    } else if (rematch("^\\ ", fs, false)) {
        strcpy(ofs, "  ");
    } else {
        strcpy(ofs, fs);
    }

    FILE *fp = get_readable_fp(file);
    int total_lines = get_lines_count(fp);

    if (total_lines > BUF_SIZE) {
        FAIL("transpose - Data contains too many lines");
    }

    /* Read the entire file into a buffer */
    fseek(fp, 0, SEEK_END);
    long file_size = ftell(fp);
    fseek(fp, 0, SEEK_SET);
    char *file_buffer = malloc(file_size + 1);
    fread(file_buffer, file_size, 1, fp);
    file_buffer[file_size] = '\0';

    /* Create a BLOCK object to hold the file contents */
    BLOCK text_buffer;
    text_buffer.start = file_buffer;
    text_buffer.end = file_buffer + file_size;

    /* Initialize variables to store the results of find_fields_in_text */
    int total_line_count = 0;
    int maximum_word_length = 0;
    fieldref *fields_table = NULL;
    int number_of_fields = 0;

    /* Find all fields in the text */
    find_fields_in_text(text_buffer, fs, file->fs->is_regex, &total_line_count,
                        &maximum_word_length, &fields_table, &number_of_fields);

    /* Transpose the fields and rows of the CSV file */
    int max_field_len = 0;
    int max_column_count = 0;

    for (int i = 0; i < number_of_fields; i++) {
        fieldref ref = fields_table[i];

        if (IS_DEBUG) {
            printf("Ref %d: %d %d %d %d\n", i, ref.start, ref.len, ref.row, ref.col);
        }

        if (ref.len > max_field_len) {
            max_field_len = ref.len;
        }

        if (ref.col >= max_column_count) {
            max_column_count = ref.col + 1;
        }
    }

    if (IS_DEBUG) {
        debug_fields_table(fields_table, file_buffer, total_line_count,
                           max_column_count, number_of_fields);
    }

    int total_len = 0;
    int fs_size = 1;

    for (int i = 0; i < max_column_count; i++) {
        for (int j = 0; j < total_line_count; j++) {
            fieldref *ref = get_ref_for_field(fields_table, number_of_fields, i, j);

            if (ref != NULL) {
                if (IS_DEBUG) {
                    printf("Ref found for %d %d: %d %d %d %d\n", i, j, ref->start,
                           ref->len, ref->row, ref->col);
                }
                total_len += ref->len;
            } else if (IS_DEBUG) {
                printf("Ref for %d %d not found, maybe empty field.\n", i, j);
            }

            if (j < total_line_count - 1) {
                total_len += fs_size;
            }
        }

        if (i < max_column_count - 1) {
            total_len += 1;
        }
    }

    /* Create a buffer to hold the transposed CSV file */
    char *transposed_buffer = malloc(total_len + 1);
    memset(transposed_buffer, ' ', total_len + 1);
    transposed_buffer[total_len] = '\0';
    total_len = 0;
    char *newline = "\n";
    DEBUG_PRINT("total_line_count = %d\n", total_line_count);

    for (int i = 0; i < max_column_count; i++) {
        for (int j = 0; j < total_line_count; j++) {
            fieldref *ref = get_ref_for_field(fields_table, number_of_fields, j, i);

            if (ref != NULL) {
                char *field = file_buffer + ref->start;
                if (IS_DEBUG) {
                    printf("%d %d %d %d\n", ref->start, ref->len, ref->row, ref->col);
                    printf("%s\n", field);
                }
                char *transposed_field = transposed_buffer + total_len;
                memcpy(transposed_field, field, ref->len);
                DEBUG_PRINT("%s\n", transposed_field);
                total_len += ref->len;
            }

            if (j < total_line_count - 1) {
                memcpy(transposed_buffer + total_len, ofs, fs_size);
                total_len += fs_size;
            }
        }

        if (i < max_column_count - 1) {
            memcpy(transposed_buffer + total_len, newline, 1);
            total_len += 1;
        }
    }

    /* Write the transposed data to standard output */
    printf("%s", transposed_buffer);
    return 0;
}
