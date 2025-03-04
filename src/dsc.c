#include "dsc.h"
#include "same-inode.h"
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

// Forward declarations
extern void cleanup_inferfs(void);
extern void cleanup_base(void);

typedef struct program {
    const char *name;
    int (*main_func)(int, char **, data_file *);
    int (*fs_func)(int, char **, data_file *);
    bool use_file;
    bool is_inferfs;
    int expected_filename_index;
    bool terminating;
} program;

program programs[] = {
    {"index", run_index, infer_field_separator, true, false, 2},
    {"inferfs", NULL, infer_field_separator, true, true, 2},
    {"random", get_random, NULL, true, false, 3},
    {"reorder", reorder, infer_field_separator, true, false, 2},
    {"fit", fit, infer_field_separator, true, false, 2},
    {"transpose", transpose, infer_field_separator, true, false, 2}
};

static program *get_step_program(char **argv) {
    if (strlen(argv[1]) > 24) {
        FAIL("dsc - Invalid method provided.");
    }
    char program_str[25];
    strcpy(program_str, argv[1]);
    int i;
    for (i = 0; i < ARRAY_SIZE(programs); i++) {
        if (strcmp(program_str, programs[i].name) == 0) {
            return &programs[i];
        }
    }
    FAIL("dsc - Invalid method provided.");
}

static data_file get_file(int argc, char **argv, program *program_to_run) {
    struct data_file file;
    file.fd = -1;
    file.tmp_file_index = -1;
    bool is_piped = !isatty(STDIN_FILENO);
    int remove_file_arg = 0;
    int expected_filename_index = program_to_run->expected_filename_index;

    // Handle some exceptional use file cases
    if (strcmp(program_to_run->name, "random") == 0) {
        program_to_run->use_file = is_piped || argc > 3;
    }

    // Get filename if provided and valid - otherwise use stdin and remove arg if necessary.
    if (program_to_run->use_file) {
        if (is_piped || argc < expected_filename_index + 1) {
            file.fd = 0;
            file.is_piped = true;
        } else if (strcmp(argv[expected_filename_index], "--") == 0) {
            remove_file_arg = 1;
            file.fd = 0;
            file.is_piped = true;
        } else {
            file.fd = open(argv[expected_filename_index], O_RDONLY);

            if (file.fd > 2) {
                remove_file_arg = 1;
                close(file.fd);
                strcpy(file.filename, argv[expected_filename_index]);
                file.is_piped = false;
            } else if (file.fd == 0) {
                file.is_piped = true;
            } else {
                FAIL("dsc - Invalid filename provided.");
            }
        }
    }

    return file;
}

// Entry point and router for the various programs.
int main(int argc, char **argv) {
    // Register cleanup functions
    if (atexit(cleanup_base) != 0 || atexit(cleanup_inferfs) != 0) {
        fprintf(stderr, "Failed to register cleanup functions\n");
        return EXIT_FAILURE;
    }

    DEBUG_PRINT(("dsc - starting main method\n"));

    if (argc == 1) {
        FAIL("dsc - No routine provided.");
    }

    int args_base_offset = 2;

    program *program_to_run = get_step_program(argv);
    program_to_run->terminating = false;
    data_file file = get_file(argc, argv, program_to_run);

    // allocate memory and copy arguments
    int args_offset = args_base_offset;
    int new_argc = argc - 1 - file.remove_file_arg;
    char **new_argv = malloc((new_argc) * sizeof *new_argv);

    for (int i = 0; i < argc - args_offset + file.remove_file_arg; ++i) {
        if (i + args_offset == program_to_run->expected_filename_index) {
            args_offset++; // skip the filename arg
            if (i == argc - args_offset) {
                break;
            }
        }
        size_t length = strlen(argv[i + args_offset]) + 1;
        new_argv[i] = malloc(length);
        memcpy(new_argv[i], argv[i + args_offset], length);
        DEBUG_PRINT(("dsc - copied arg from main argv: %s\n", new_argv[i]));
    }

    new_argv[argc - args_offset] = NULL;

    if (program_to_run->fs_func && program_to_run->use_file) {
        program_to_run->fs_func(new_argc, new_argv, &file);

        if (program_to_run->is_inferfs) {
            printf("%s", file.fs->sep);
            return 0;
        }
    }

    bool dev_null_output = false;
    bool possibly_tty = false;
    struct stat tmp_stat;
    if (fstat(STDOUT_FILENO, &tmp_stat) == 0) {
        if (S_ISCHR(tmp_stat.st_mode)) {
            struct stat null_stat;
            if (stat("/dev/null", &null_stat) == 0 && SAME_INODE(tmp_stat, null_stat))
                dev_null_output = true;
            else
                possibly_tty = true;
        }
    }

    int result = program_to_run->main_func(new_argc, new_argv, &file);

    int clear_files = clear_temp_files();
    if (clear_files == 1) {
        DEBUG_PRINT(("dsc - Cleared any temporary files.\n"));
    } else if (clear_files == -1) {
        DEBUG_PRINT(("dsc - Failed to clear temporary files.\n"));
    }

    return result;
}
