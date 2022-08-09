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

  int (*mainFunc)(int, char **, data_file *);
  int (*fsFunc)(int, char **, data_file *) = inferFieldSeparator;
  char program[25];
  strcpy(program, argv[1]);
  int argsBaseOffset = 2;
  bool useFile = true; // If not using filename, set to false below
  int inferfs = 0;

  if (strcmp(program, PROGRAM_INDEX) == 0) {
    mainFunc = runIndex;
  } else if (strcmp(program, PROGRAM_IFS) == 0) {
    mainFunc = NULL;
    inferfs = 1;
  } else {
    fail("Invalid method provided.");
    return 1;
  }

  int argsOffset = argsBaseOffset + useFile;
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

  int newArgc = argc - 1 - useFile;
  char **newArgv = malloc((newArgc) * sizeof *newArgv);

  for (int i = 0; i < argc - argsOffset; ++i) {
    size_t length = strlen(argv[i + argsOffset]) + 1;
    newArgv[i] = malloc(length);
    memcpy(newArgv[i], argv[i + argsOffset], length);
    DEBUG_PRINT(("%s\n", newArgv[i]));
  }

  newArgv[argc - argsOffset] = NULL;

  if (fsFunc && useFile) {
    fsFunc(newArgc, newArgv, &file);

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

  mainFunc(newArgc, newArgv, &file);

  /*
  // free memory
  for(int i = 0; i < argc; ++i) {
      free(newArgv[i]);
  }
  free(newArgv);
  */

  return 0;
}
