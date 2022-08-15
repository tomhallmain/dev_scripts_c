#include "dsc.h"
#include "same-inode.h"
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

int main(int argc, char **argv) {
  DEBUG_PRINT(("%s\n", "starting main method"));

  if (argc == 1) {
    fail("No routine provided.");
  }

  int (*main_func)(int, char **, data_file *);
  int (*fsFunc)(int, char **, data_file *) = infer_field_separator;
  char program[25];
  strcpy(program, argv[1]);
  int args_base_offset = 2;
  bool useFile = true; // If not using filename, set to false below
  int inferfs = 0;

  if (strcmp(program, PROGRAM_INDEX) == 0) {
    main_func = run_index;
  } else if (strcmp(program, PROGRAM_IFS) == 0) {
    main_func = NULL;
    inferfs = 1;
  } else {
    fail("Invalid method provided.");
    return 1;
  }

  int args_offset = args_base_offset + useFile;
  struct data_file file;
  file.fd = -1;

  // Get filename if provided and valid - otherwise use stdin
  if (useFile) {
    if (argc > 2 && !(strcmp(argv[2], "--") == 0)) {
      file.fd = open(argv[2], O_RDONLY);

      if (file.fd > 2) {
        close(file.fd);
        strcpy(file.filename, argv[2]);
        useFile = true;
      } else {
        useFile = false;
      }
    } else {
      file.fd = 0;
    }
  } else {
    useFile = false;
  }

  // allocate memory and copy arguments

  int new_argc = argc - 1 - useFile;
  char **new_argv = malloc((new_argc) * sizeof *new_argv);

  for (int i = 0; i < argc - args_offset; ++i) {
    size_t length = strlen(argv[i + args_offset]) + 1;
    new_argv[i] = malloc(length);
    memcpy(new_argv[i], argv[i + args_offset], length);
    DEBUG_PRINT(("%s\n", new_argv[i]));
  }

  new_argv[argc - args_offset] = NULL;

  if (fsFunc && useFile) {
    fsFunc(new_argc, new_argv, &file);

    if (inferfs) {
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

  main_func(new_argc, new_argv, &file);

  /*
  // free memory
  for(int i = 0; i < argc; ++i) {
      free(new_argv[i]);
  }
  free(new_argv);
  */

  return 0;
}
