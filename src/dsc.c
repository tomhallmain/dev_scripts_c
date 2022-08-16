#include "dsc.h"
#include "same-inode.h"
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

// Entry point and router for the various programs.
int main(int argc, char **argv) {
  DEBUG_PRINT(("%s\n", "starting main method"));

  if (argc == 1) {
    FAIL("No routine provided.");
  }

  int (*main_func)(int, char **, data_file *);
  int (*fsFunc)(int, char **, data_file *) = infer_field_separator;
  char program[25];
  strcpy(program, argv[1]);
  int args_base_offset = 2;
  bool use_file = true; // If not using filename, set to false below
  bool run_inferfs = true;
  bool is_piped = !isatty(STDIN_FILENO);
  int expected_filename_index = 2;
  int is_inferfs = 0;

  if (strcmp(program, PROGRAM_INDEX) == 0) {
    main_func = run_index;
  } else if (strcmp(program, PROGRAM_IFS) == 0) {
    main_func = NULL;
    is_inferfs = 1;
  } else if (strcmp(program, PROGRAM_RANDOM) == 0) {
    main_func = get_random;
    run_inferfs = false;
    expected_filename_index = 3;
    use_file = is_piped || argc > 3;
  } else {
    FAIL("Invalid method provided.");
  }

  struct data_file file;
  file.fd = -1;
  int remove_file_arg = 0;

  // Get filename if provided and valid - otherwise use stdin and remove arg if
  // necessary.
  if (use_file) {
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

  if (fsFunc && use_file && run_inferfs) {
    fsFunc(new_argc, new_argv, &file);

    if (is_inferfs) {
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

  int result = main_func(new_argc, new_argv, &file);

  /*
  // free memory
  for(int i = 0; i < argc; ++i) {
      free(new_argv[i]);
  }
  free(new_argv);
  */

  return result;
}
