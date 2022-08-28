#include "dsc.h"
#include "map1.h"
#include <regex.h>
#include <stdlib.h>
#include <string.h>

typedef struct key2 {
  int i;
  int j;
} key2;

static map1 buf = NULL;

static void set_map() { buf = map_create(); }

static void get_or_save_field(int i, int j, bool set[BUF_SIZE][MAX_NF_COUNT],
                              char *line, int start, int end) {
  if (start < 0) {
    if (!set[i][j]) {
      return;
    }
    key2 key;
    key.i = i;
    key.j = j;
    hex_dump("transpose - key\n", &key, sizeof(key));
    char *val = (char *)map_get(buf, &key);
    stpcpy(line, val);
  } else {
    key2 key;
    key.i = i;
    key.j = j;
    hex_dump("transpose - key\n", &key, sizeof(key));
    char value[4];
    // char value[end - start + 1];
    //  char *field_val = (char *)malloc(sizeof(char) * (end - start + 1));
    stpcpy(value, substr(line, start, end));
    printf("%s\n", value);
    uintptr_t addr = (uintptr_t)malloc(sizeof(value));
    map_set(buf, &key, addr);
    hex_dump("transpose - value\n", &addr, sizeof(value));
    set[key.i][key.j] = true;
  }
}

// Transpose the field values of a text-based field-separated file.
int transpose(int argc, char **argv, data_file *file) {
  DEBUG_PRINT(("transpose - Running transpose\n"));
  char fs[15];
  char ofs[15];
  strcpy(fs, file->fs->sep);

  set_map();

  // hashmap *map1 = hashmap_create();
  //
  //  if (1) {
  //    struct key2 test1;
  //    test1.i = 0;
  //    test1.j = 0;
  //    char aaa[4] = "aaa";
  //    map_put(map1, &test1, (uintptr_t)&aaa);
  //    printf("%p\n", &test1);
  //    printf("%lu\n", (uintptr_t)&aaa);
  //    printf("%p\n", &aaa);
  //    printf("%s\n", (char *)(uintptr_t)&aaa);
  //  }
  //
  //  if (2) {
  //    struct key2 test2;
  //    test2.i = 0;
  //    test2.j = 0;
  //    char bbb[4];
  //    uintptr_t b_ptr;
  //    map_get(map1, &test2, &b_ptr);
  //    printf("%p\n", &test2);
  //    printf("%lu\n", b_ptr);
  //    printf("%p\n", (char *)b_ptr);
  //    printf("%s\n", (char *)b_ptr);
  //    bucket_dump(map1);
  //    exit(0);
  //  }

  // adjust field separator for special cases
  if (rematch("\\[\\[:space:\\]\\]\\{2.\\}", fs, false)) {
    strcpy(ofs, "  ");
  } else if (rematch("\\[.+\\]", fs, false)) {
    strcpy(ofs, " ");
  } else if (rematch("^\\ ", fs, false)) {
    strcpy(ofs, "  ");
  } else {
    strcpy(ofs, fs);
  }

  FILE *fp = get_readable_fp(file);
  int total_lines = get_lines_count(fp);

  if (total_lines > BUF_SIZE) {
    FAIL("transpose - Data contains too many lines");
  }

  bool set[BUF_SIZE][MAX_NF_COUNT];
  char line[BUF_SIZE];
  int n_valid_rows = 0;
  int line_counter = 0;
  int max_nf = 0;
  bool is_regex = file->fs->is_regex;
  regex_t regex;
  if (is_regex) {
    regex = get_compiled_regex(file->fs->sep, false);
  }

  for (int i = 0; i < BUF_SIZE; i++) {
    for (int j = 0; j < MAX_NF_COUNT; j++) {
      set[i][j] = false;
    }
  }

  while (fgets(line, BUF_SIZE, fp) != NULL) {
    // Remove trailing newline
    line[strcspn(line, "\n")] = 0;
    ++line_counter;

    if (rematch(EMPTY_LINE_RE, line, true)) {
      continue;
    }

    size_t len = strlen(line);
    int nf = 0;

    if (is_regex) {
      int position = 0;
      while (position < len) {
        regmatch_t pmatch[1];
        int err = regexec(&regex, &line[position], 1, pmatch, 0);
        if (!err) {
          int end = (int)pmatch[0].rm_so;
          get_or_save_field(n_valid_rows, nf, set, line, position, end);
          nf++;
          position += (int)pmatch[0].rm_eo;
          if (nf > MAX_NF_COUNT) {
            break;
          }
        } else if (err == REG_NOMATCH) {
          // DEBUG_PRINT(("transpose - key %p\n", &key));
          get_or_save_field(n_valid_rows, nf, set, line, position, len);
          DEBUG_PRINT(("transpose - Saved field \"%s\"\n", &line[position]));
          // uintptr_t test_ptr;
          // map_get(buf, &keys[n_valid_rows][nf], &test_ptr);
          // printf("transpose - %d %d %p\n", keys[n_valid_rows][nf].i,
          //        keys[n_valid_rows][nf].j, &keys[n_valid_rows][nf]);
          // printf("transpose - get \"%s\"\n", (char *)test_ptr);
          break;
        } else {
          printf("transpose - %d\n", err);
          FAIL("transpose - Regex match failed.");
        }
      }
    }

    if (nf > max_nf) {
      max_nf = nf;
    }

    ++n_valid_rows;
  }

  printf("transpose - valid rows: %d max_nf: %d\n", n_valid_rows, max_nf);
  // bucket_dump(buf);
  // bucket_dump_regex();

  for (int i = 0; i < max_nf + 1; i++) {
    for (int j = 0; j < n_valid_rows; j++) {
      if (set[j][i]) {
        char field[4];
        get_or_save_field(j, i, set, field, -1, -1);
        printf("%s", field);
      }
      if (j < n_valid_rows - 1) {
        printf("%s", ofs);
      }
    }

    printf("\n");
  }

  map_destroy(buf);

  return 0;
}
