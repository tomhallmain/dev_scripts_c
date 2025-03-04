#include "dsc.h"
#include <fcntl.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>

// Print data with an index.
int run_index(int argc, char **argv, data_file *file) {
    DEBUG_PRINT(("%s\n", "Running index"));
    FILE *fp = get_readable_fp(file);
    char fs[15];
    bool space_fs = false;

    // adjust field separator for special cases
    if (rematch("\\[\\[:space:\\]\\]\\{2.\\}", file->fs->sep, false)) {
        strcpy(fs, "  ");
        space_fs = true;
    } else if (rematch("\\[.+\\]", file->fs->sep, false)) {
        strcpy(fs, " ");
        space_fs = true;
    } else if (rematch("^\\ ", file->fs->sep, false)) {
        strcpy(fs, "  ");
        space_fs = true;
    } else {
        strcpy(fs, file->fs->sep);
    }

    char *f_str;

    if (space_fs) {
        int total_lines = get_lines_count(fp);
        int index_len = get_int_char_len(total_lines);
        printf("%s", "");
        char *index_len_str = int_to_char(index_len);

        const char *f_start = "%";
        const char *f_end = "d%s%s\n";
        f_str = (char *)malloc(1 + strlen(f_start) + index_len + strlen(f_end));
        strcpy(f_str, f_start);
        strcat(f_str, index_len_str);
        strcat(f_str, f_end);
    } else {
        f_str = "%d%s%s\n";
    }

    int ind = 0;
    char buffer[BUF_SIZE];
    while (fgets(buffer, BUF_SIZE, fp) != NULL) {
        // Remove trailing newline
        buffer[strcspn(buffer, "\n")] = 0;
        printf(f_str, ++ind, fs, buffer);
    }

    fclose(fp);
    if (space_fs) {
        free(f_str);
    }
    return 0;
}
