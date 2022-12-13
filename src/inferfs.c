#include "dsc.h"
#include "map.h"
#include "map1.h"
#include <math.h>
#include <regex.h>
#include <stdlib.h>
#include <string.h>

#define CSV_SUFFIX ".csv"
#define TSV_SUFFIX ".tsv"
#define PROPERTIES_SUFFIX ".properties"

static field_sep CommonFS[] = {
    {'s', SPACE, false, 0, 0, 0, 0},     {'t', TAB, false, 0, 0, 0, 0},
    {'p', PIPE, false, 0, 0, 0, 0},      {'m', SEMICOLON, false, 0, 0, 0, 0},
    {'o', COLON, false, 0, 0, 0, 0},     {'c', COMMA, false, 0, 0, 0, 0},
    {'e', "=", false, 0, 0, 0, 0},       {'w', SPACEMULTI, true, 0, 0, 0, 0},
    {'z', SPACEMULTI_, true, 0, 0, 0, 0}};

int COMMON_FS_COUNT = ARRAY_SIZE(CommonFS);
static field_sep ds_sep = {'@', DS_SEP, false, 0, 0, 0, 0};
static hashmap *QuoteREs = NULL;

struct re {
  char *pattern;
};

static field_sep *get_common_fs(char key) {
  for (int i = 0; i < COMMON_FS_COUNT; i++) {
    if (CommonFS[i].key == key) {
      return &CommonFS[i];
    }
  }
  UNREACHABLE("No common field sep found for this key.");
}

static void set_static_maps() {
  if (!QuoteREs) {
    QuoteREs = hashmap_create();
  }
}

static struct re get_quoted_fields_re(field_sep fs, char *q) {
  // TODO: CRLF in fields
  uintptr_t quote_re;
  char qsep_key[17];
  stpcpy(stpcpy(qsep_key, q), &fs.key);
  if (IS_DEBUG)
    printf("%s\n", qsep_key);

  if (map_get_(QuoteREs, qsep_key, &quote_re)) {
    struct re re = *(struct re *)quote_re;
    if (IS_DEBUG)
      printf("After was: %zul %s\n", quote_re, (char *)re.pattern);
    return re;
  }

  char sep_search[16];
  if (strcmp(fs.sep, PIPE) == 0) {
    stpcpy(stpcpy(sep_search, BACKSLASH), fs.sep);
  } else {
    stpcpy(sep_search, fs.sep);
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
  struct re re;
  char pattern[230];
  nstpcpy(pattern, re_arr, 22);
  re.pattern = pattern;
  uintptr_t test = (uintptr_t)malloc(sizeof(re));
  map_put_(QuoteREs, qsep_key, test);

  if (IS_DEBUG) {
    printf("%s Before was: %zul\n", re.pattern, test);
  }

  return re;
}

static uintptr_t get_fields_quote(char *line, field_sep fs) {
  char dqre[230];
  if (IS_DEBUG)
    printf("Getting quote RE 2\n");
  get_quoted_fields_re(fs, (char *)DOUBLEQUOT);
  if (rematch(dqre, line, true)) {
    return (uintptr_t)2;
  }
  char sqre[230];
  if (IS_DEBUG)
    printf("Getting quote RE 1\n");
  get_quoted_fields_re(fs, (char *)SINGLEQUOT);
  if (rematch(sqre, line, true)) {
    return (uintptr_t)1;
  }
  return (uintptr_t)0;
}

// TODO
static int count_matches_for_quoted_field_line(char *re, char *line) {
  return 0;
}

// Infer a field separator from data.
int infer_field_separator(int argc, char **argv, data_file *file) {
  if (!file->is_piped) {
    if (endswith(file->filename, CSV_SUFFIX)) {
      file->fs = get_common_fs('c');
      return 0;
    } else if (endswith(file->filename, TSV_SUFFIX)) {
      file->fs = get_common_fs('t');
      return 0;
    } else if (endswith(file->filename, PROPERTIES_SUFFIX)) {
      file->fs = get_common_fs('e');
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

  if (IS_DEBUG)
    printf("Quotes: %p QuoteREs: %p\n", &Quotes, &QuoteREs);

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
    ++line_counter;

    printf("newline\n");
    printf("Testing empty line RE\n");
    if (rematch(EMPTY_LINE_RE, line, true)) {
      continue;
    }

    ++n_valid_rows;

    if (n_valid_rows >= max_rows) {
      break;
    }

    if (n_valid_rows < 10 && strstr(line, DS_SEP)) {
      file->fs = &ds_sep;
      return 0;
    }

    const size_t len = strlen(line);

    for (int s = 0; s < COMMON_FS_COUNT; s++) {
      field_sep *fs = &CommonFS[s];

      // Handle quoting
      char *quote;
      uintptr_t n_quotes;

      DEBUG_PRINT(("Getting quote \"%s\"\n", fs->sep));

      if (!map_get_(Quotes, &fs->key, &n_quotes)) {
        n_quotes = get_fields_quote(line, *fs);
        if (n_quotes) {
          map_put_(Quotes, &fs->key, n_quotes);
        }
      }

      int nf = 0;

      if (n_quotes) {
        quote = (n_quotes == 1) ? SINGLEQUOT : DOUBLEQUOT;
        struct re re = get_quoted_fields_re(*fs, quote);
        nf = count_matches_for_quoted_field_line(re.pattern, line);
      } else if (fs->is_regex) {
        nf = count_matches_for_line_regex(fs->sep, line, len);
      } else {
        nf = count_matches_for_line_char(fs->sep[0], line, len);
      }

      if (IS_DEBUG) {
        printf("inferfs - Value of nf for sep \"%s\" : %d\n", fs->sep, nf);
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
      printf("inferfs - average_nf %f\n", average_nf);
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
        printf("inferfs - %d %c %d %f\n", j, fs->key, (int)row_nf_count,
               point_var);
      }
      fs->sum_var += point_var;
    }

    if ((double)n_valid_rows / applicable_rows_count > 6) {
      continue;
    }

    fs->var = fs->sum_var / max_rows;

    if (IS_DEBUG) {
      printf("inferfs - %c %f %f\n", fs->key, average_nf, fs->var);
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
      printf("inferfs - Winner: %d \"%s\"\n", winning_s,
             CommonFS[winning_s].sep);
    } else {
      printf("inferfs - No winner identified, using \"%s\"\n", SPACEMULTI);
    }
  }

  // rewind(fp);
  // fclose(fp);

  if (winning_s < 0) {
    file->fs = get_common_fs('w');
    return 1;
  } else {
    file->fs = &CommonFS[winning_s];
    return 0;
  }

  return 0;
}
