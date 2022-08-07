#include "dsc.h"
#include <fcntl.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <unistd.h>

int runIndex(int argc, char **argv, char *filename, int fd, char *fs) {
  FILE *fp;

  DEBUG_PRINT(("%s\n", "Running index"));

  if (fd > 2) {
    fp = fopen(filename, "r");
  } else {
    fd = stdincpy();
    fp = fdopen(fd, "r");
  }

  int space_fs = 0;

  if (regexMatch("\\[\\[:space:\\]\\]\\{2.\\}", fs)) {
    fs = "  ";
    space_fs = 1;
  } else if (regexMatch("\\[.+\\]", fs)) {
    fs = " ";
    space_fs = 1;
  } else if (regexMatch("^\\ ", fs)) {
    fs = "  ";
    space_fs = 1;
  }

  int totalLines = getLinesCount(fp);
  int indLen = getIntCharLen(totalLines);
  printf("%s", "");
  char *indSpLen = intToChar(indLen);

  char *printfStart = "%";
  char *printfEnd = "d  %s\n";
  char *printfStr =
      (char *)malloc(1 + strlen(printfStart) + indLen + strlen(printfEnd));
  strcpy(printfStr, printfStart);
  strcat(printfStr, indSpLen);
  strcat(printfStr, printfEnd);

  int ind = 0;
  char buffer[BUF_SIZE];
  while (fgets(buffer, BUF_SIZE, fp) != NULL) {
    // Remove trailing newline
    buffer[strcspn(buffer, "\n")] = 0;
    printf(printfStr, ++ind, buffer);
  }

  fclose(fp);
  free(printfStr);
  return 0;
}
