#include "dsc.h"
#include "map.h"
#include <math.h>
#include <regex.h>
#include <stdlib.h>
#include <string.h>

#define CSV_SUFFIX ".csv"
#define TSV_SUFFIX ".tsv"
#define PROPERTIES_SUFFIX ".properties"
#define COMMON_FS_COUNT 8
#define MAX_NF_COUNT 80

static char *CommonFSOrder[COMMON_FS_COUNT] = {"s", "t", "p", "m",
                                               "o", "c", "w", "2w"};
static hashmap *CommonFS = NULL;
static hashmap *QuoteREs = NULL;

static int endswith(const char *str, const char *suffix) {
  if (!str || !suffix)
    return 0;
  size_t lenstr = strlen(str);
  size_t lensuffix = strlen(suffix);
  if (lensuffix > lenstr)
    return 0;
  return strncmp(str + lenstr - lensuffix, suffix, lensuffix) == 0;
}

static void set_static_maps() {
  if (!CommonFS) {
    CommonFS = hashmap_create();
    map_put_(CommonFS, CommonFSOrder[0], (uintptr_t)&SPACE);
    map_put_(CommonFS, CommonFSOrder[1], (uintptr_t)&TAB);
    map_put_(CommonFS, CommonFSOrder[2], (uintptr_t)&PIPE);
    map_put_(CommonFS, CommonFSOrder[3], (uintptr_t)&SEMICOLON);
    map_put_(CommonFS, CommonFSOrder[4], (uintptr_t)&COLON);
    map_put_(CommonFS, CommonFSOrder[5], (uintptr_t)&COMMA);
    map_put_(CommonFS, CommonFSOrder[6], (uintptr_t)&SPACEMULTI);
    map_put_(CommonFS, CommonFSOrder[7], (uintptr_t)&SPACEMULTI_);
  }
  if (!QuoteREs) {
    QuoteREs = hashmap_create();
  }
}

static void get_quoted_fields_re(char *rebuf, const char *sep, char *q) {
  // TODO: CRLF in fields
  uintptr_t quote_re;
  char qsep_key[17];
  stpcpy(stpcpy(qsep_key, q), sep);

  if (map_get_(QuoteREs, qsep_key, &quote_re)) {
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
  map_put_(QuoteREs, qsep_key, (uintptr_t)malloc(sizeof(rebuf)));
}

static uintptr_t get_fields_quote(char *line, const char *sep) {
  char dqre[230];
  get_quoted_fields_re(dqre, sep, (char *)DOUBLEQUOT);
  if (rematch(dqre, line, true)) {
    return (uintptr_t)2;
  }
  char sqre[230];
  get_quoted_fields_re(sqre, sep, (char *)SINGLEQUOT);
  if (rematch(sqre, line, true)) {
    return (uintptr_t)1;
  }
  return (uintptr_t)0;
}

// TODO
static int count_matches_for_quoted_field_line(char *re, char *line) {
  return 0;
}

static int count_matches_for_line(const char *sep, char *line, size_t len,
                                  bool is_char) {
  int count = 0;

  if (is_char) {
    ++count;

    for (int i = 0; i < len; i++) {
      if (line[i] == sep[0]) {
        ++count;
      }
    }
  } else {
    int position = 0;
    regex_t regex;
    // TODO figure out why get_compiled_regex isn't working here
    regcomp(&regex, sep, REG_EXTENDED);

    while (position < len) {
      regmatch_t pmatch[1];
      int err = regexec(&regex, &line[position], 1, pmatch, 0);
      if (!err) {
        ++count;
        position += (int)pmatch[0].rm_eo;
        if (count > 500) {
          break;
        }
      } else if (err == REG_NOMATCH) {
        break;
      } else {
        printf("%d\n", err);
        FAIL("Regex match FAILed.");
      }
    }

    count--;
  }

  return count;
}

// Infer a field separator from data.
int infer_field_separator(int argc, char **argv, data_file *file) {
  if (!file->is_piped) {
    const char *filename = file->filename;

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
  }

  set_static_maps();

  FILE *fp;
  if (file->fd > 2) {
    fp = fopen(file->filename, "r");
  } else if (file->is_piped) {
    file->fd = stdincpy();
    fp = fdopen(file->fd, "r");
  } else {
    FAIL("No filename provided.");
  }

  int n_valid_rows = 0;
  int max_rows = 500;
  char line[BUF_SIZE];
  int line_counter = 0;
  char *empty_line_re = "^ *$";
  hashmap *Quotes = hashmap_create();
  //  hashmap *CommonFSCount = hashmap_create();
  //  hashmap *CommonFSNFConsecCounts = hashmap_create();
  int CommonFSCount1[COMMON_FS_COUNT][max_rows];
  int CommonFSNFConsecCounts1[COMMON_FS_COUNT][MAX_NF_COUNT];
  int CommonFSTotal[COMMON_FS_COUNT];
  int PrevNF[COMMON_FS_COUNT];

  for (int i = 0; i < COMMON_FS_COUNT; i++) {
    PrevNF[i] = 0;
    CommonFSTotal[i] = 0;

    for (int j = 0; j < max_rows; j++) {
      CommonFSCount1[i][j] = 0;

      if (j < MAX_NF_COUNT) {
        CommonFSNFConsecCounts1[i][j] = 0;
      }
    }
  }

  while (fgets(line, BUF_SIZE, fp) != NULL) {
    // Remove trailing newline
    line[strcspn(line, "\n")] = 0;

    if (rematch(empty_line_re, line, true)) {
      continue;
    }

    ++n_valid_rows;

    if (n_valid_rows >= max_rows) {
      break;
    }

    if (n_valid_rows < 10 && strstr(line, DS_SEP)) {
      file->fs = DS_SEP;
      return 0;
    }

    const size_t len = strlen(line);

    for (int s = 0; s < COMMON_FS_COUNT; s++) {
      char *sep_key = CommonFSOrder[s];
      uintptr_t sep_ptr;
      map_get_(CommonFS, sep_key, &sep_ptr);
      const char *sep = (char *)sep_ptr;

      // Handle quoting
      char *quote;
      uintptr_t n_quotes;

      if (!map_get_(Quotes, sep_key, &n_quotes)) {
        n_quotes = get_fields_quote(line, sep);
        if (n_quotes) {
          map_put_(Quotes, sep_key, n_quotes);
        }
      }

      int nf = 0;

      if (n_quotes) {
        quote = (n_quotes == 1) ? SINGLEQUOT : DOUBLEQUOT;
        char re[230];
        get_quoted_fields_re(re, sep, quote);
        nf = count_matches_for_quoted_field_line(re, line);
      } else {
        nf = count_matches_for_line(sep, line, len, s < 6);

        if (IS_DEBUG) {
          printf("Value of nf for sep \"%s\" : %d\n", sep, nf);
        }
      }

      // TODO maybe remove
      if (nf < 2)
        continue;

      CommonFSCount1[s][line_counter] = nf;
      CommonFSTotal[s] += nf;
      int prev_nf = PrevNF[s];

      if (prev_nf && nf != prev_nf) {
        int consec_count = CommonFSNFConsecCounts1[s][prev_nf];

        if (consec_count) {
          if (consec_count < 3) {
            CommonFSNFConsecCounts1[s][prev_nf] = 0;
          }
        }
      }

      PrevNF[s] = nf;
      CommonFSNFConsecCounts1[s][nf] = CommonFSNFConsecCounts1[s][nf] + 1;
    }

    ++line_counter;
  }

  if (max_rows > n_valid_rows)
    max_rows = n_valid_rows;

  double SumVar[COMMON_FS_COUNT];
  double FSVar[COMMON_FS_COUNT];
  char *NoVar[COMMON_FS_COUNT];
  char *Winners[COMMON_FS_COUNT];
  int winning_s = -1;

  for (int s = 0; s < COMMON_FS_COUNT; s++) {
    char *sep_key = CommonFSOrder[s];
    float average_nf = (float)CommonFSTotal[s] / max_rows;

    if (average_nf < 2)
      continue;

    if (IS_DEBUG) {
      printf("average_nf %f\n", average_nf);
    }

    int applicable_rows_count = 0;

    for (int j = 0; j < max_rows; j++) {
      int row_nf_count = CommonFSCount1[s][j];

      if (!row_nf_count) {
        continue;
      }

      ++applicable_rows_count;
      double point_var = pow(row_nf_count - average_nf, 2);
      if (IS_DEBUG) {
        printf("%d %s %d %f\n", j, sep_key, (int)row_nf_count, point_var);
      }
      SumVar[s] += point_var;
    }

    if ((double)n_valid_rows / applicable_rows_count > 6) {
      continue;
    }

    FSVar[s] = SumVar[s] / max_rows;

    if (IS_DEBUG) {
      printf("%s %f %f\n", sep_key, average_nf, FSVar[s]);
    }

    if (FSVar[s] == 0) {
      NoVar[s] = CommonFSOrder[s];
      winning_s = s;
      Winners[s] = CommonFSOrder[s]; // TODO remove
    } else if (winning_s < 0 || FSVar[s] < FSVar[winning_s]) {
      winning_s = s;
      Winners[s] = CommonFSOrder[s]; // TODO remove
    }
  }

  if (IS_DEBUG) {
    printf("Winner: %s\n", Winners[winning_s]);
  }

  if (winning_s < 0) {
    file->fs = SPACEMULTI;
    return 1;
  } else {
    char *sep_key = CommonFSOrder[winning_s];
    uintptr_t sep_ptr;
    map_get_(CommonFS, sep_key, &sep_ptr);
    char *sep = (char *)sep_ptr;
    file->fs = sep;
    return 0;
  }

  // rewind(fp);

  return 0;
}
