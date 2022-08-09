#include "dsc.h"
#include <fcntl.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <unistd.h>

int runIndex(int argc, char **argv, data_file *file) {
  FILE *fp;

  DEBUG_PRINT(("%s\n", "Running index"));

  if (file->fd > 2) {
    fp = fopen(file->filename, "r");
  } else {
    file->fd = stdincpy();
    fp = fdopen(file->fd, "r");
  }

  char fs[6];
  strcpy(fs, file->fs);
  bool space_fs = false;

  // adjust field separator for special cases

  if (rematch("\\[\\[:space:\\]\\]\\{2.\\}", fs)) {
    strcpy(fs, "  ");
    space_fs = true;
  } else if (rematch("\\[.+\\]", fs)) {
    strcpy(fs, " ");
    space_fs = true;
  } else if (rematch("^\\ ", fs)) {
    strcpy(fs, "  ");
    space_fs = true;
  }

  char *printfStr;

  if (space_fs) {
    int totalLines = getLinesCount(fp);
    int indLen = getIntCharLen(totalLines);
    printf("%s", "");
    char *indSpLen = intToChar(indLen);

    const char *printfStart = "%";
    const char *printfEnd = "d%s%s\n";
    printfStr =
        (char *)malloc(1 + strlen(printfStart) + indLen + strlen(printfEnd));
    strcpy(printfStr, printfStart);
    strcat(printfStr, indSpLen);
    strcat(printfStr, printfEnd);
  } else {
    printfStr = "%d%s%s\n";
  }

  int ind = 0;
  char buffer[BUF_SIZE];
  while (fgets(buffer, BUF_SIZE, fp) != NULL) {
    // Remove trailing newline
    buffer[strcspn(buffer, "\n")] = 0;
    printf(printfStr, ++ind, fs, buffer);
  }

  fclose(fp);
  if (space_fs) {
    free(printfStr);
  }
  return 0;
}
