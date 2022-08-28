#include "dsc.h"
#include "map.h"
#include <regex.h>
#include <stdlib.h>
#include <string.h>

#define MAX_TEMP_FILES 20

static int tmp_file_index = 0;
typedef struct tmp_file {
  char *filename;
  int fd;
} tmp_file;
static tmp_file tmp_files[MAX_TEMP_FILES];

static hashmap *regex_map = NULL;

// Common utilities methods

static int stdincpy(data_file *file) {
  if (file->tmp_file_index > -1) {
    UNREACHABLE("base - stdin should already be copied to a temporary file");
  } else if (tmp_file_index == MAX_TEMP_FILES - 1) {
    FAIL("base - Too many temporary files");
  }

  char template[21] = "/tmp/ds_stdin_XXXXXX";
  tmp_file temp;
  temp.fd = mkstemp(template);
  char *filename = malloc(sizeof(char) * (strlen(template) + 1));
  stpcpy(filename, template);
  temp.filename = filename;
  file->tmp_file_index = tmp_file_index;
  tmp_files[tmp_file_index++] = temp;

  DEBUG_PRINT(("base - using stdin, copying to temporary file\n"));
  DEBUG_PRINT(("base - %s\n", tmp_files[file->tmp_file_index].filename));

  if (feof(stdin))
    printf("base - stdin reached eof\n");

  FILE *fp = fdopen(temp.fd, "w");

  if (fp == 0)
    printf("base - something went wrong opening temp file\n");

  unsigned long nbytes;
  char buffer[BUF_SIZE];

  while ((nbytes = fread(buffer, 1, BUF_SIZE, stdin))) {
    fwrite(buffer, nbytes, 1, fp);
  }

  if (ferror(stdin))
    printf("base - error reading from stdin");

  rewind(fp);
  return temp.fd;
}

FILE *get_readable_fp(data_file *file) {
  if (file->is_piped) {
    if (file->tmp_file_index < 0) {
      int fd = stdincpy(file);
      return fdopen(fd, "r");
    } else {
      return fdopen(tmp_files[file->tmp_file_index].fd, "r");
    }
  } else if (file->fd > 2) {
    return fopen(file->filename, "r");
  } else {
    printf("base - Filename: %s fd: %d \n", file->filename, file->fd);
    FAIL("base - Failed to get readable file pointer.");
  }
}

int clear_temp_files(void) {
  int result = 0;
  for (int i = tmp_file_index - 1; i >= 0; i--) {
    if (remove(tmp_files[i].filename)) {
      result = -1;
    } else if (result > -1) {
      result = 1;
    }
    free(tmp_files[i].filename);
    tmp_files[i].fd = -1;
  }
  return result;
}

// if (checkStdin()) {
//     fp = tmpfile();
//     char ch;
//     while((ch = fgetc(stdin)) != EOF) {
//         fputc(ch, fp);
//     }
//     rewind(fp);
// }
// static bool check_stdin(void) {
//   DEBUG_PRINT(("%s\n", "in checkStdin"));
//   int ch = getchar();
//   if (ch == EOF) {
//     DEBUG_PRINT(("%s\n", "char was EOF"));
//     if (!(feof(stdin))) {
//       puts("stdin error"); // very rare
//     }
//     return false;
//   } else {
//     DEBUG_PRINT(("%s\n", "char was not EOF"));
//     ungetc(ch, stdin); // put back
//     DEBUG_PRINT(("%s\n", "char put back"));
//     return true;
//   }
// }

void nstpcpy(char *buff, char *strings[], int len) {
  for (int i = 0; i < len; i++) {
    buff = stpcpy(buff, strings[i]);
  }
}

int get_lines_count(FILE *fp) {
  int totalLines = 0;

  if (fp == NULL) {
    DEBUG_PRINT(("%s\n", "fp was NULL"));
    FAIL("Error while opening the file.");
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

char *int_to_char(int x) {
  char *x_char;

  if (sprintf(x_char, "%d", x) == -1) {
    FAIL("sprintf");
  } else {
    return x_char;
  }
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

int endswith(const char *str, const char *suffix) {
  if (!str || !suffix)
    return 0;
  size_t lenstr = strlen(str);
  size_t lensuffix = strlen(suffix);
  if (lensuffix > lenstr)
    return 0;
  return strncmp(str + lenstr - lensuffix, suffix, lensuffix) == 0;
}

void bucket_dump_regex() { bucket_dump(regex_map); }

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
        DEBUG_PRINT(
            ("base - Failed to compile regex for pattern \"%s\" with error %d",
             pattern, compile_err));
        FAIL("base - rematch error - could not compile regex");
      }

      map_put_(regex_map, pattern, (uintptr_t)malloc(sizeof(regex)));
    }
  } else {
    int compile_err = regcomp(&regex, pattern, REG_EXTENDED);

    if (compile_err) {
      DEBUG_PRINT(
          ("base - Failed to compile regex for pattern \"%s\" with error %d",
           pattern, compile_err));
      FAIL("base - rematch error - could not compile regex");
    }
  }

  return regex;
}

int rematch(char *pattern, char *test_string, bool reuse) {
  regex_t regex = get_compiled_regex(pattern, reuse);
  int result = regexec(&regex, test_string, 0, NULL, 0);
  return result ? 0 : 1;
}

// Simple character count
int count_matches_for_line_char(const char sep_char, char *line, size_t len) {
  int count = 1;

  for (int i = 0; i < len; i++) {
    if (line[i] == sep_char) {
      ++count;
    }
  }

  return count;
}

int count_matches_for_line_str(const char *sep, char *line, size_t len) {
  int count = 0;
  size_t sep_len = strlen(sep);
  bool is_char = sep_len == 1;

  if (is_char) {
    return count_matches_for_line_char(sep[0], line, len);
  } else {
    // Fixed string count
    for (int i = 0; i < len; i++) {
      bool all_match = true;
      int j;
      for (j = 0; j < sep_len; j++) {
        if (line[i + j] != sep[j]) {
          all_match = false;
        }
      }
      if (all_match) {
        count++;
        i += j - 1;
      }
    }
  }

  return count;
}

int count_matches_for_line_regex(const char *sep, char *line, size_t len) {
  // Regex count
  int count = 0;
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
      printf("base - %d\n", err);
      FAIL("base - Regex match failed.");
    }
  }

  if (count > 0)
    count--;

  return count;
}

void hex_dump(char *desc, void *addr, int len) {
  int i;
  unsigned char buff[17];
  unsigned char *pc = (unsigned char *)addr;

  // Output description if given.
  if (desc != NULL)
    printf("%s:\n", desc);

  // Process every byte in the data.
  for (i = 0; i < len; i++) {
    // Multiple of 16 means new line (with line offset).

    if ((i % 16) == 0) {
      // Just don't print ASCII for the zeroth line.
      if (i != 0)
        printf("  %s\n", buff);

      // Output the offset.
      printf("  %04x ", i);
    }

    // Now the hex code for the specific character.
    printf(" %02x", pc[i]);

    // And store a printable ASCII character for later.
    if ((pc[i] < 0x20) || (pc[i] > 0x7e)) {
      buff[i % 16] = '.';
    } else {
      buff[i % 16] = pc[i];
    }

    buff[(i % 16) + 1] = '\0';
  }

  // Pad out last line if not exactly 16 characters.
  while ((i % 16) != 0) {
    printf("   ");
    i++;
  }

  // And print the final ASCII bit.
  printf("  %s\n", buff);
}
