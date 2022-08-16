#include "dsc.h"
#include <fcntl.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>

// Print data with an index.
int run_index(int argc, char **argv, data_file *file) {
  DEBUG_PRINT(("%s\n", "Running index"));
  FILE *fp;

  if (file->fd > 2) {
    fp = fopen(file->filename, "r");
  } else if (file->is_piped) {
    file->fd = stdincpy();
    fp = fdopen(file->fd, "r");
  } else {
    FAIL("No filename provided.");
  }

  char fs[6];
  strcpy(fs, file->fs);
  bool space_fs = false;

  // adjust field separator for special cases

  if (rematch("\\[\\[:space:\\]\\]\\{2.\\}", fs, false)) {
    strcpy(fs, "  ");
    space_fs = true;
  } else if (rematch("\\[.+\\]", fs, false)) {
    strcpy(fs, " ");
    space_fs = true;
  } else if (rematch("^\\ ", fs, false)) {
    strcpy(fs, "  ");
    space_fs = true;
  }

  char *printfStr;

  if (space_fs) {
    int totalLines = get_lines_count(fp);
    int indLen = get_int_char_len(totalLines);
    printf("%s", "");
    char *indSpLen = int_to_char(indLen);

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
