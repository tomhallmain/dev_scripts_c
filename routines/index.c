#include "dsc.h"
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <unistd.h>

int runIndex(int argc, char **argv, char *filename, char *fs) {
  FILE *fp;

  DEBUG_PRINT(("%s\n", "Running index"));
  DEBUG_PRINT(("%s\n", filename));

  if (filename) {
    fp = fopen(filename, "r");
  } else {
    printf("Enter name of a file you wish to index:\n");
    scanf("%s", filename);
    fp = fopen(filename, "r");
  }

  int totalLines = getLinesCount(fp);
  int indLen = getIntCharLen(totalLines);
  printf("%s", "");
  char *indSpLen = intToChar(indLen);

  char *printfStart = "%";
  char *printfEnd = "d  %s\n";
  char *printfStr =
      (char *)malloc(1 + strlen(printfStart) + indLen + strlen(&printfEnd));
  strcpy(printfStr, printfStart);
  strcat(printfStr, indSpLen);
  strcat(printfStr, printfEnd);

  int ind = 0;
  char buffer[MAX_LEN];
  while (fgets(buffer, MAX_LEN, fp) != NULL) {
    // Remove trailing newline
    buffer[strcspn(buffer, "\n")] = 0;
    printf(printfStr, ++ind, buffer);
  }

  fclose(fp);
  free(printfStr);
  return 0;
}
