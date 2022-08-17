#include "dsc.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#define MODE_NUM 0
#define MODE_TEXT 1
#define MODE_ERR 2

void seed_random() {
  struct timespec start;
  clock_gettime(CLOCK_MONOTONIC_RAW, &start);
  srand(start.tv_sec + start.tv_nsec);
}

double get_random_double_one_to_zero() {
  double rand1 = (double)rand();
  double rand2 = (double)rand();

  if (rand1 > rand2) {
    return rand2 / rand1;
  } else {
    return rand1 / rand2;
  }
}

// Get a random number or do soft text anonymization for generating random text
// data.
int get_random(int argc, char **argv, data_file *file) {
  int mode = MODE_ERR;
  DEBUG_PRINT(("%s\n", "Running random"));

  if (argc == 0) {
    mode = file->is_piped ? MODE_TEXT : MODE_NUM;
  } else {
    char *mode_str = argv[0];
    if (!mode_str || strcmp(mode_str, "") == 0) {
      mode = file->is_piped ? MODE_TEXT : MODE_NUM;
    } else {
      size_t size = strlen(mode_str);
      if (size > 6) {
        FAIL("[mode] not understood - available options: [number|text]");
      }

      char mode_pattern[8];
      stpcpy(mode_pattern, "^");
      stpcpy(mode_pattern, mode_str);

      if (rematch(mode_pattern, "number", false)) {
        mode = MODE_NUM;
      } else if (rematch(mode_pattern, "text", false)) {
        mode = MODE_TEXT;
      }
    }
  }

  if (mode == MODE_NUM) {
    seed_random();
    printf("%f", get_random_double_one_to_zero());
    return 0;
  } else if (mode == MODE_ERR) {
    FAIL("[mode] not understood - available options: [number|text]");
  }

  FILE *fp = get_readable_fp(file);
  int chars_seen = 0;
  int c;

  while ((c = fgetc(fp)) != EOF) {
    if (chars_seen++ % 150 == 0) {
      seed_random();
    }
    if (c < 48 || c > 122) {
      printf("%c", (char)c);
    } else if ((c > 58 && c < 65) || (c > 90 && c < 97)) {
      printf("%c", (char)c);
    } else if (c > 96) {
      printf("%c", ((int)(get_random_double_one_to_zero() * (121 - 97))) + 97);
    } else if (c < 58) {
      printf("%c", ((int)(get_random_double_one_to_zero() * (57 - 48))) + 48);
    } else {
      printf("%c", ((int)(get_random_double_one_to_zero() * (90 - 65))) + 65);
    }
  }

  return 0;
}
