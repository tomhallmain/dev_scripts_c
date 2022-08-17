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

typedef struct field_sep {
  char key;
  char *sep;
  int total;
  double sum_var;
  double var;
  int prev_nf;
} field_sep;
static field_sep CommonFS[] = {
    {'s', SPACE, 0, 0, 0, 0},      {'t', TAB, 0, 0, 0, 0},
    {'p', PIPE, 0, 0, 0, 0},       {'m', SEMICOLON, 0, 0, 0, 0},
    {'o', COLON, 0, 0, 0, 0},      {'c', COMMA, 0, 0, 0, 0},
    {'w', SPACEMULTI, 0, 0, 0, 0}, {'z', SPACEMULTI_, 0, 0, 0, 0}};

static hashmap *QuoteREs = NULL;

static void set_static_maps() {
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
        FAIL("Regex match failed.");
      }
    }

    if (count > 0)
      count--;
  }

  return count;
}

// Infer a field separator from data.
int infer_field_separator(int argc, char **argv, data_file *file) {
  if (!file->is_piped) {
    if (endswith(file->filename, CSV_SUFFIX)) {
      file->fs = ",";
      return 0;
    } else if (endswith(file->filename, TSV_SUFFIX)) {
      file->fs = "\t";
      return 0;
    } else if (endswith(file->filename, PROPERTIES_SUFFIX)) {
      file->fs = "=";
      return 0;
    }
  }

  set_static_maps();

  FILE *fp = get_readable_fp(file);
  int n_valid_rows = 0;
  int max_rows = 500;
  char line[BUF_SIZE];
  int line_counter = 0;
  hashmap *Quotes = hashmap_create();
  int CommonFSCount[COMMON_FS_COUNT][max_rows];
  int CommonFSNFConsecCounts[COMMON_FS_COUNT][MAX_NF_COUNT];

  // Init base values for multiarrays
  for (int i = 0; i < COMMON_FS_COUNT; i++) {
    for (int j = 0; j < max_rows; j++) {
      CommonFSCount[i][j] = 0;

      if (j < MAX_NF_COUNT) {
        CommonFSNFConsecCounts[i][j] = 0;
      }
    }
  }

  while (fgets(line, BUF_SIZE, fp) != NULL) {
    // Remove trailing newline
    line[strcspn(line, "\n")] = 0;

    if (rematch(EMPTY_LINE_RE, line, true)) {
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
      field_sep *fs = &CommonFS[s];

      // Handle quoting
      char *quote;
      uintptr_t n_quotes;

      if (!map_get_(Quotes, &fs->key, &n_quotes)) {
        n_quotes = get_fields_quote(line, fs->sep);
        if (n_quotes) {
          map_put_(Quotes, &fs->key, n_quotes);
        }
      }

      int nf = 0;

      if (n_quotes) {
        quote = (n_quotes == 1) ? SINGLEQUOT : DOUBLEQUOT;
        char re[230];
        get_quoted_fields_re(re, &fs->key, quote);
        nf = count_matches_for_quoted_field_line(re, line);
      } else {
        nf = count_matches_for_line(fs->sep, line, len, s < 6);

        if (IS_DEBUG) {
          printf("Value of nf for sep \"%s\" : %d\n", fs->sep, nf);
        }
      }

      // TODO maybe remove
      if (nf < 2)
        continue;

      CommonFSCount[s][line_counter] = nf;
      fs->total += nf;
      int prev_nf = fs->prev_nf;

      if (prev_nf && nf != prev_nf) {
        int consec_count = CommonFSNFConsecCounts[s][prev_nf];

        if (consec_count) {
          if (consec_count < 3) {
            CommonFSNFConsecCounts[s][prev_nf] = 0;
          }
        }
      }

      fs->prev_nf = nf;
      CommonFSNFConsecCounts[s][nf]++;
    }

    ++line_counter;
  }

  if (max_rows > n_valid_rows)
    max_rows = n_valid_rows;

  field_sep *NoVar[COMMON_FS_COUNT];
  int winning_s = -1;

  for (int s = 0; s < COMMON_FS_COUNT; s++) {
    field_sep *fs = &CommonFS[s];
    float average_nf = (float)fs->total / max_rows;

    if (average_nf < 2)
      continue;

    if (IS_DEBUG) {
      printf("average_nf %f\n", average_nf);
    }

    int applicable_rows_count = 0;

    for (int j = 0; j < max_rows; j++) {
      int row_nf_count = CommonFSCount[s][j];

      if (!row_nf_count) {
        continue;
      }

      ++applicable_rows_count;
      double point_var = pow(row_nf_count - average_nf, 2);
      if (IS_DEBUG) {
        printf("%d %c %d %f\n", j, fs->key, (int)row_nf_count, point_var);
      }
      fs->sum_var += point_var;
    }

    if ((double)n_valid_rows / applicable_rows_count > 6) {
      continue;
    }

    fs->var = fs->sum_var / max_rows;

    if (IS_DEBUG) {
      printf("%c %f %f\n", fs->key, average_nf, fs->var);
    }

    if (fs->var == 0) {
      NoVar[s] = fs;
      winning_s = s;
    } else if (winning_s < 0 || fs->var < CommonFS[winning_s].var) {
      winning_s = s;
    }
  }

  if (IS_DEBUG) {
    if (winning_s > -1) {
      printf("Winner: %d \"%s\"\n", winning_s, CommonFS[winning_s].sep);
    } else {
      printf("No winner identified, using \"%s\"\n", SPACEMULTI);
    }
  }

  // rewind(fp);
  //  fclose(fp);

  if (winning_s < 0) {
    file->fs = SPACEMULTI;
    return 1;
  } else {
    file->fs = CommonFS[winning_s].sep;
    return 0;
  }

  return 0;
}
