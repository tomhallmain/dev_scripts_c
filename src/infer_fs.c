#include "dsc.h"
#include "map.h"
#include <stdlib.h>
#include <string.h>

#define CSV_SUFFIX ".csv"
#define TSV_SUFFIX ".tsv"
#define PROPERTIES_SUFFIX ".properties"
#define COMMON_FS_COUNT 8

static char *commonfsorder[COMMON_FS_COUNT] = {"s", "t", "p", "m",
                                               "o", "c", "w", "2w"};
static hashmap *commonfs = NULL;

static int endswith(const char *str, const char *suffix) {
  if (!str || !suffix)
    return 0;
  size_t lenstr = strlen(str);
  size_t lensuffix = strlen(suffix);
  if (lensuffix > lenstr)
    return 0;
  return strncmp(str + lenstr - lensuffix, suffix, lensuffix) == 0;
}

static void set_maps() {
  if (!commonfs) {
    commonfs = hashmap_create();
    map_put_(commonfs, commonfsorder[0], (uintptr_t)&SPACE);
    map_put_(commonfs, commonfsorder[1], (uintptr_t)&TAB);
    map_put_(commonfs, commonfsorder[2], (uintptr_t)&PIPE);
    map_put_(commonfs, commonfsorder[3], (uintptr_t)&SEMICOLON);
    map_put_(commonfs, commonfsorder[4], (uintptr_t)&COLON);
    map_put_(commonfs, commonfsorder[5], (uintptr_t)&COMMA);
    map_put_(commonfs, commonfsorder[6], (uintptr_t)&SPACEMULTI);
    map_put_(commonfs, commonfsorder[7], (uintptr_t)&SPACEMULTI_);
  }
}

static void get_quoted_fields_re(hashmap *quote_res, char *rebuf, char *sep,
                                 char *q) { // TODO: CRLF in fields
  uintptr_t quote_re;
  char qsep_key[17];
  stpcpy(stpcpy(qsep_key, q), sep);

  if (map_get_(quote_res, qsep_key, &quote_re)) {
    stpcpy(rebuf, (char *)quote_re);
  }

  char sep_search[16];
  if (strcmp(sep, PIPE) == 0) {
    stpcpy(stpcpy(sep_search, BACKSLASH), sep);
  } else {
    stpcpy(sep_search, sep);
  }

  char qs[20]; // quote + sep
  stpcpy(stpcpy(qs, q), sep_search);
  char spq[20]; // sep + quote
  stpcpy(stpcpy(spq, sep_search), q);
  char *exc_arr[7] = {"[^", q, "]*[^", sep_search, "]*[^", q, "]+"};
  char exc[50];
  nstpcpy(exc, exc_arr, 7);
  char *re_arr[22] = {"(^", q,  qs,  "|", spq, qs, "|", spq, q,   "$|", q,
                      exc,  qs, "|", spq, exc, qs, "|", spq, exc, q,    "$)"};
  nstpcpy(rebuf, re_arr, 22);
  map_put_(quote_res, qsep_key, (uintptr_t)malloc(sizeof(rebuf)));
}

static int get_fields_quote(hashmap *quote_res, char *line, char *sep) {
  char dqre[230];
  get_quoted_fields_re(quote_res, dqre, sep, (char *)DOUBLEQUOT);
  if (rematch(dqre, line)) {
    return 2;
  }
  char sqre[230];
  get_quoted_fields_re(quote_res, sqre, sep, (char *)SINGLEQUOT);
  if (rematch(sqre, line)) {
    return 1;
  }
  return 0;
}

/* main method */
int inferFieldSeparator(int argc, char **argv, data_file *file) {
  char *filename = file->filename;

  set_maps();

  int n_valid_rows = 0;
  hashmap *quote_res = hashmap_create();
  hashmap *quotes = hashmap_create();
  int max_rows = 500;
  FILE *fp;

  if (file->fd > 2) {
    fp = fopen(file->filename, "r");
  } else {
    file->fd = stdincpy();
    fp = fdopen(file->fd, "r");
  }

  char line[BUF_SIZE];
  int line_counter = 0;
  while (fgets(line, BUF_SIZE, fp) != NULL) {
    // Remove trailing newline
    line[strcspn(line, "\n")] = 0;
    size_t len = strlen(line);
    ++line_counter;
    printf("%d\n", line_counter);

    for (int s = 0; s < COMMON_FS_COUNT; s++) {
      char *sep_key = commonfsorder[s];
      uintptr_t sep_ptr;
      map_get_(commonfs, sep_key, &sep_ptr);
      char *sep = (char *)sep_ptr;

      // Handle quoting
      char *quote;
      uintptr_t n_quotes;

      if (!map_get_(quotes, sep_key, &n_quotes)) {
        n_quotes = get_fields_quote(quote_res, line, sep);
        if (n_quotes) {
          map_put_(quotes, sep_key, n_quotes);
        }
      }
      if (n_quotes) {
        quote = (n_quotes == 1) ? SINGLEQUOT : DOUBLEQUOT;
        char re[230];
        get_quoted_fields_re(quote_res, re, sep, quote);
      }
    }
  }

  // rewind(fp);

  // TODO move this back to top
  if (endswith(filename, CSV_SUFFIX)) {
    file->fs = ",";
    return 0;
  } else if (endswith(filename, TSV_SUFFIX)) {
    file->fs = "\t";
    return 0;
  } else if (endswith(filename, PROPERTIES_SUFFIX)) {
    file->fs = "=";
    return 0;
  }

  return 0;
}
