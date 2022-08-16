#include "dsc.h"
#include <stdlib.h>

// Transpose the field values of a text-based field-separated file

int transpose(int argc, char **argv, data_file *file) {
  DEBUG_PRINT(("%s\n", "Running index"));
  FILE *fp;

  if (file->fd > 2) {
    fp = fopen(file->filename, "r");
  } else if (file->is_piped) {
    file->fd = stdincpy();
    fp = fdopen(file->fd, "r");
  } else {
    FAIL("No filename provided.");
  }

  return 0;
}
