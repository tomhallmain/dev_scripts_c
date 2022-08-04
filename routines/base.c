#include "dsc.h"
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

char *intToChar(int x) {
  char *x_char;

  if (sprintf(x_char, "%d", x) == -1) {
    perror("sprintf");
  }

  return x_char;
}

char *inferFieldSeparator(char *filename) { return " "; }

// if (checkStdin()) {
//     fp = tmpfile();
//     char ch;
//     while((ch = fgetc(stdin)) != EOF) {
//         fputc(ch, fp);
//     }
//     rewind(fp);
// }

bool checkStdin() {
  DEBUG_PRINT(("%s\n", "in checkStdin"));
  int ch = getchar();
  DEBUG_PRINT(("%s\n", "got char"));
  if (ch == EOF) {
    DEBUG_PRINT(("%s\n", "char was EOF"));
    if (!(feof(stdin))) {
      puts("stdin error"); // very rare
    }
    return false;
  } else {
    DEBUG_PRINT(("%s\n", "char was not EOF"));
    ungetc(ch, stdin); // put back
    DEBUG_PRINT(("%s\n", "char ungotten"));
    return true;
  }
}

int getLinesCount(FILE *fp) {
  int totalLines = 0;

  if (fp == NULL) {
    perror("Error while opening the file.\n");
    exit(EXIT_FAILURE);
  }

  char chr = getc(fp);
  while (chr != EOF) {
    if (chr == '\n') {
      totalLines++;
    }
    chr = getc(fp);
  }

  rewind(fp);
  return totalLines;
}

int getIntCharLen(int input) {
  int len = 1, test10sPlace = 10, negative = 0;
  if (input < 0) {
    negative = 1;
    test10sPlace *= -1;
    while (input <= test10sPlace) {
      test10sPlace *= 10;
      len++;
    }
    test10sPlace *= -1;
  } else {
    while (input >= test10sPlace) {
      test10sPlace *= 10;
      len++;
    }
  }
  return len + negative;
}
