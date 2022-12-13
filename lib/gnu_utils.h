#include <string.h>

#define SWALLOW_REALLOC_LOG 12

/* A BLOCK delimit a region in memory of arbitrary size, like the copy of a
   whole file.  A WORD is something smaller, its length should fit in a
   short integer.  A WORD_TABLE may contain several WORDs.  */

typedef struct {
  char *start; /* pointer to beginning of region */
  char *end;   /* pointer to end + 1 of region */
} BLOCK;

typedef struct {
  char *start; /* pointer to beginning of region */
  short size;  /* length of the region */
} WORD;

typedef struct {
  WORD *start;   /* array of WORDs */
  size_t length; /* number of entries */
} WORD_TABLE;
