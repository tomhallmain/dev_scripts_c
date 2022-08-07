#include "dsc.h"
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

int main(int argc, char **argv) {
  DEBUG_PRINT(("%s\n", "starting main method"));

  if (argc == 1) {
    fail("No routine provided.");
  }

  int (*mainFunc)(int, char **, char *, int fd, char *);
  char *(*fsFunc)(int, char **, char *, int fd) = inferFieldSeparator;
  char program[25];
  strcpy(program, argv[1]);
  int argsBaseOffset = 2;
  int useFile = 1; // If not using filename, set to 0 below
  int fd = -1;
  int inferfs = 0;

  if (match(program, PROGRAM_INDEX)) {
    mainFunc = runIndex;
  } else if (match(program, PROGRAM_IFS)) {
    mainFunc = NULL;
    inferfs = 1;
  } else {
    perror("Invalid method provided.");
    return 1;
  }

  int argsOffset = argsBaseOffset + useFile;
  char filename[25];

  // Get filename if provided and valid - otherwise use stdin
  if (useFile) {
    if (argc > 2 && !(match(argv[2], "--"))) {
      fd = open(argv[2], O_RDONLY);

      if (fd > 2) {
        close(fd);
        strcpy(filename, argv[2]);
        useFile = 1;
      } else {
        useFile = 0;
      }
    } else {
      fd = 0;
    }
  } else {
    useFile = 0;
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
  char *fs = NULL;

  if (fsFunc && useFile) {
    fs = fsFunc(newArgc, newArgv, filename, fd);

    if (inferfs) {
      printf("%s", fs);
      return 0;
    }
  }

  mainFunc(newArgc, newArgv, filename, fd, fs);

  /*
  // free memory
  for(int i = 0; i < argc; ++i) {
      free(newArgv[i]);
  }
  free(newArgv);
  */

  return 0;
}
