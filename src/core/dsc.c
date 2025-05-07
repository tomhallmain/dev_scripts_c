#include "../include/dsc.h"
#include "../include/same-inode.h"
#include "../include/posix_compat.h"
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
    int (*main_func)(int, char **, data_file_t *);
    int (*fs_func)(int, char **, data_file_t *);
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

static program *get_step_program(const char *name) {
    for (int i = 0; i < ARRAY_SIZE(programs); i++) {
        if (strcmp(name, programs[i].name) == 0) {
            return &programs[i];
        }
    }
    FAIL("dsc - Invalid method provided.");
}

// Parse a command string into argc/argv format
// Format: command[args] | command[args] | ...
static void parse_command(const char *cmd, int *argc, char ***argv) {
    char *cmd_copy = strdup(cmd);
    char *token = strtok(cmd_copy, " ");
    int count = 0;
    char **args = NULL;

    while (token) {
        args = realloc(args, (count + 1) * sizeof(char *));
        args[count++] = strdup(token);
        token = strtok(NULL, " ");
    }

    *argc = count;
    *argv = args;
    free(cmd_copy);
}

// Entry point and router for the various programs.
int main(int argc, char **argv) {
    // Register cleanup functions
    if (atexit(cleanup_base) != 0 || atexit(cleanup_inferfs) != 0) {
        fprintf(stderr, "Failed to register cleanup functions\n");
        return EXIT_FAILURE;
    }

    DEBUG_PRINT("dsc - starting main method");

    if (argc == 1) {
        FAIL("dsc - No routine provided.");
    }

    // Check if we're part of a pipe
    bool is_piped_input = !isatty(STDIN_FILENO);
    bool is_piped_output = !isatty(STDOUT_FILENO);

    // Get the command to run
    program *program_to_run = get_step_program(argv[1]);
    program_to_run->terminating = false;

    // Set up the data file structure
    data_file_t file = {0};
    file.fd = -1;
    file.tmp_file_index = -1;
    file.is_piped = is_piped_input;

    // If we have piped input, we need to handle it appropriately
    if (is_piped_input) {
        program_to_run->use_file = true;
        file.fd = 0;  // Use stdin
    } else if (program_to_run->use_file && argc > program_to_run->expected_filename_index) {
        // Handle regular file input
        const char *filename = argv[program_to_run->expected_filename_index];
        if (strcmp(filename, "--") == 0) {
            file.fd = 0;
            file.is_piped = true;
        } else {
            file.fd = open(filename, O_RDONLY);
            if (file.fd <= 0) {
                FAIL("dsc - Invalid filename provided.");
            }
            strcpy(file.filename, filename);
        }
    }

    // Prepare arguments for the program
    int new_argc = argc - (program_to_run->use_file ? 3 : 2);
    char **new_argv = malloc((new_argc + 1) * sizeof(char *));
    
    // Copy relevant arguments, skipping program name and command name
    for (int i = 0; i < new_argc; i++) {
        size_t len = strlen(argv[i + 2]) + 1;
        new_argv[i] = malloc(len);
        memcpy(new_argv[i], argv[i + 2], len);
    }
    new_argv[new_argc] = NULL;

    // Run field separator inference if needed
    if (program_to_run->fs_func && program_to_run->use_file) {
        program_to_run->fs_func(new_argc, new_argv, &file);
        if (program_to_run->is_inferfs) {
            printf("%s", file.fs->sep);
            return 0;
        }
    }

    // Execute the program
    int result = program_to_run->main_func(new_argc, new_argv, &file);

    // Clean up
    for (int i = 0; i < new_argc; i++) {
        free(new_argv[i]);
    }
    free(new_argv);

    // Clean up temporary files
    int clear_files = dsc_clear_temp_files();
    if (clear_files == 1) {
        DEBUG_PRINT("dsc - Cleared any temporary files.");
    } else if (clear_files == -1) {
        DEBUG_PRINT("dsc - Failed to clear temporary files.");
    }

    return result;
}
