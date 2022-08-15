#include "dsc.h"
#include "map.h"
#include <regex.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static hashmap *regex_map = NULL;

char *int_to_char(int x) {
  char *x_char;

  if (sprintf(x_char, "%d", x) == -1) {
    fail("sprintf");
  } else {
    return x_char;
  }
}

// Following function extracts characters present in `src`
// between `m` and `n` (excluding `n`)
char *substr(const char *src, int m, int n) {
  int len = n - m;
  // allocate (len + 1) chars for destination
  char *dest = (char *)malloc(sizeof(char) * (len + 1));

  // extract characters between m'th and n'th index from source string
  // and copy them into the destination string
  for (int i = m; i < n && (*(src + i) != '\0'); i++) {
    *dest = *(src + i);
    dest++;
  }

  // null-terminate the destination string
  *dest = '\0';
  return dest - len;
}

regex_t get_compiled_regex(char *pattern, bool reuse) {
  regex_t regex;

  if (reuse) {
    if (!regex_map) {
      regex_map = hashmap_create();
    }

    uintptr_t pregex;

    if (map_get_(regex_map, pattern, &pregex)) {
      regex = *(regex_t *)pregex;
    } else {
      int compile_err = regcomp(&regex, pattern, REG_EXTENDED);

      if (compile_err) {
        DEBUG_PRINT(("Failed to compile regex for pattern \"%s\" with error %d",
                     pattern, compile));
        fail("rematch error - could not compile regex");
      }

      map_put_(regex_map, pattern, (uintptr_t)malloc(sizeof(regex)));
    }
  } else {
    int compile_err = regcomp(&regex, pattern, REG_EXTENDED);

    if (compile_err) {
      DEBUG_PRINT(("Failed to compile regex for pattern \"%s\" with error %d",
                   pattern, compile));
      fail("rematch error - could not compile regex");
    }
  }

  return regex;
}

int rematch(char *pattern, char *test_string, bool reuse) {
  regex_t regex = get_compiled_regex(pattern, reuse);
  int result = regexec(&regex, test_string, 0, NULL, 0);
  return result ? 0 : 1;
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

bool check_stdin() {
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

int get_lines_count(FILE *fp) {
  int totalLines = 0;

  if (fp == NULL) {
    DEBUG_PRINT(("%s\n", "fp was NULL"));
    fail("Error while opening the file.");
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

int get_int_char_len(int input) {
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

void nstpcpy(char *buff, char *strings[], int len) {
  for (int i = 0; i < len; i++) {
    buff = stpcpy(buff, strings[i]);
  }
}
