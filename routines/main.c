#include "dsc.h"
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

int main(int argc, char **argv) {
  DEBUG_PRINT(("%s\n", "starting main method"));

  if (argc == 1) {
    perror("No routine provided.");
    return 1;
  }

  int (*mainFunc)(int, char **, char *, char *);
  char *(*fsFunc)(char *);
  char program[25];
  strcpy(program, argv[1]);
  int argsBaseOffset = 2;
  int useFile = 1; // If not using filename, set to 0 below

  if (match(program, PROGRAM_INDEX)) {
    mainFunc = runIndex;
    fsFunc = inferFieldSeparator;
  } else {
    perror("Invalid method provided.");
    return 1;
  }

  int argsOffset = argsBaseOffset + useFile;
  char filename[25];

  // Get filename if provided and valid - otherwise use stdin
  if (useFile && argc > 1 && !(match(argv[2], "--"))) {
    useFile = open(argv[2], O_RDONLY);

    if (useFile > 1) {
      close(useFile);
      strcpy(filename, argv[2]);
      useFile = 1;
    } else {
      useFile = 0;
    }
  } else {
    useFile = 0;
  }

  // allocate memory and copy strings

  char **new_argv = malloc((argc - 1 - useFile) * sizeof *new_argv);

  for (int i = 0; i < argc - argsOffset; ++i) {
    size_t length = strlen(argv[i + argsOffset]) + 1;
    new_argv[i] = malloc(length);
    memcpy(new_argv[i], argv[i + argsOffset], length);
    DEBUG_PRINT(("%s\n", new_argv[i]));
  }

  new_argv[argc - argsOffset] = NULL;
  char *fs = NULL;

  if (fsFunc && useFile) {
    fs = fsFunc(filename);
  }

  // do operations on new_argv
  mainFunc(argc - 2, new_argv, filename, fs);

  /*
  // free memory
  for(int i = 0; i < argc; ++i) {
      free(new_argv[i]);
  }
  free(new_argv);
  */

  return 0;
}
