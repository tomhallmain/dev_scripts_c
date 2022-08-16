#include "dsc.h"
#include "same-inode.h"
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

struct program {
  const char *name;
  int (*main_func)(int, char **, data_file *);
  int (*fsFunc)(int, char **, data_file *);
  bool use_file;
  bool is_inferfs;
  bool run_inferfs;
  int expected_filename_index;
};
struct program programs[] = {
    {"index", run_index, infer_field_separator, true, false, true, 2},
    {"inferfs", NULL, infer_field_separator, true, true, true, 2},
    {"random", get_random, NULL, true, false, false, 3},
    {"reorder", reorder, infer_field_separator, true, false, true, 2},
    {"fit", fit, infer_field_separator, true, false, true, 2},
    {"transpose", transpose, infer_field_separator, true, false, true, 2}};

static struct program *get_step_program(char **argv) {
  char program_str[25];
  strcpy(program_str, argv[1]);
  int i;
  for (i = 0; i < ARRAY_SIZE(programs); i++) {
    if (strcmp(program_str, programs[i].name) == 0) {
      return &programs[i];
    }
  }
  FAIL("Invalid method provided.");
}

// Entry point and router for the various programs.
int main(int argc, char **argv) {
  DEBUG_PRINT(("%s\n", "starting main method"));

  if (argc == 1) {
    FAIL("No routine provided.");
  }

  int args_base_offset = 2;
  bool is_piped = !isatty(STDIN_FILENO);

  struct program *program_to_run = get_step_program(argv);
  int expected_filename_index = program_to_run->expected_filename_index;

  struct data_file file;
  file.fd = -1;
  int remove_file_arg = 0;

  // Get filename if provided and valid - otherwise use stdin and remove arg if
  // necessary.
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
        FAIL("Invalid filename provided.");
      }
    }
  }

  // allocate memory and copy arguments

  int args_offset = args_base_offset;
  int new_argc = argc - 1 - remove_file_arg;
  char **new_argv = malloc((new_argc) * sizeof *new_argv);

  for (int i = 0; i < argc - args_offset + remove_file_arg; ++i) {
    if (i + args_offset == expected_filename_index) {
      args_offset++; // skip the filename arg
      if (i == argc - args_offset) {
        break;
      }
    }
    size_t length = strlen(argv[i + args_offset]) + 1;
    new_argv[i] = malloc(length);
    memcpy(new_argv[i], argv[i + args_offset], length);
    DEBUG_PRINT(("copied arg from main argv: %s\n", new_argv[i]));
  }

  new_argv[argc - args_offset] = NULL;

  if (program_to_run->fsFunc && program_to_run->use_file &&
      program_to_run->run_inferfs) {
    program_to_run->fsFunc(new_argc, new_argv, &file);

    if (program_to_run->is_inferfs) {
      printf("%s", file.fs);
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

  /*
  // free memory
  for(int i = 0; i < argc; ++i) {
      free(new_argv[i]);
  }
  free(new_argv);
  */

  return result;
}
