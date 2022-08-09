#include "dsc.h"
#include <regex.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

char *intToChar(int x) {
  char *x_char;

  if (sprintf(x_char, "%d", x) == -1) {
    fail("sprintf");
  } else {
    return x_char;
  }
}

int rematch(char *pattern, char *testString) {
  regex_t regex;
  int reti;
  DEBUG_PRINT(("%s\n", "rematch"));
  reti = regcomp(&regex, pattern, REG_EXTENDED);

  if (reti) {
    DEBUG_PRINT(("%s\n", "rematchErr"));
    fail("Could not compile regex\n");
  }

  reti = regexec(&regex, testString, 0, NULL, 0);
  return reti ? 0 : 1;
}

int stdincpy(void) {
  char template[18] = "/tmp/stdin_XXXXXX";
  int tempfd = mkstemp(template);

  DEBUG_PRINT(("%s\n", "using stdin, copying to temporary file"));
  DEBUG_PRINT(("%s\n", template));

  if (feof(stdin))
    printf("stdin reached eof\n");

  FILE *fp = fdopen(tempfd, "w");

  if (fp == 0)
    printf("something went wrong opening temp file\n");

  unsigned long nbytes;
  char buffer[BUF_SIZE];

  while ((nbytes = fread(buffer, 1, BUF_SIZE, stdin))) {
    fwrite(buffer, nbytes, 1, fp);
  }

  if (ferror(stdin))
    printf("error reading from stdin");

  rewind(fp);
  return tempfd;
}

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
  if (ch == EOF) {
    DEBUG_PRINT(("%s\n", "char was EOF"));
    if (!(feof(stdin))) {
      puts("stdin error"); // very rare
    }
    return false;
  } else {
    DEBUG_PRINT(("%s\n", "char was not EOF"));
    ungetc(ch, stdin); // put back
    DEBUG_PRINT(("%s\n", "char put back"));
    return true;
  }
}

int getLinesCount(FILE *fp) {
  int totalLines = 0;

  if (fp == NULL) {
    DEBUG_PRINT(("%s\n", "fp was NULL"));
    fail("Error while opening the file.\n");
  }

  int chr = getc(fp);
  while (chr != EOF) {
    if (chr == '\n') {
      totalLines++;
    }
    chr = getc(fp);
  }

  if (fileno(fp) > 2) {
    rewind(fp);
  }

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

char *nstpcpy(char *buff, char *strings[], int len) {
  for (int i = 0; i < len; i++) {
    buff = stpcpy(buff, strings[i]);
  }
}
