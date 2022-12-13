#include "gnu_utils.h"
#include "../lib/ialloc.h"
#include "../lib/xalloc.h"
#include "error.h"

#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

/*------------------------------------------------------------------------.
| This routine will attempt to swallow a whole file name FILE_NAME into a |
| contiguous region of memory and return a description of it into BLOCK.  |
| Standard input is assumed whenever FILE_NAME is NULL, empty or "-".     |
|                                                                         |
| Previously, in some cases, white space compression was attempted while  |
| inputting text.  This was defeating some regexps like default end of    |
| sentence, which checks for two consecutive spaces.  If white space      |
| compression is ever reinstated, it should be in output routines.        |
`------------------------------------------------------------------------*/

static void swallow_file_in_memory(const char *file_name, BLOCK *block) {
  int file_handle;         /* file descriptor number */
  struct stat stat_block;  /* stat block for file */
  size_t allocated_length; /* allocated length of memory buffer */
  size_t used_length;      /* used length in memory buffer */
  int read_length;         /* number of character gotten on last read */

  /* As special cases, a file name which is NULL or "-" indicates standard
     input, which is already opened.  In all other cases, open the file from
     its name.  */

  if (!file_name || !*file_name || strcmp(file_name, "-") == 0)
    file_handle = fileno(stdin);
  else if ((file_handle = open(file_name, O_RDONLY)) < 0)
    error(EXIT_FAILURE, errno, file_name);

  /* If the file is a plain, regular file, allocate the memory buffer all at
     once and swallow the file in one blow.  In other cases, read the file
     repeatedly in smaller chunks until we have it all, reallocating memory
     once in a while, as we go.  */

  if (fstat(file_handle, &stat_block) < 0)
    error(EXIT_FAILURE, errno, file_name);

#if !MSDOS

  /* On MSDOS, we cannot predict in memory size from file size, because of
     end of line conversions.  */

  if (S_ISREG(stat_block.st_mode)) {
    block->start = (char *)xmalloc((size_t)stat_block.st_size);

    if (read(file_handle, block->start, (size_t)stat_block.st_size) !=
        stat_block.st_size)
      error(EXIT_FAILURE, errno, file_name);

    block->end = block->start + stat_block.st_size;
  } else

#endif /* not MSDOS */

  {
    block->start = (char *)xmalloc((size_t)1 << SWALLOW_REALLOC_LOG);
    used_length = 0;
    allocated_length = (1 << SWALLOW_REALLOC_LOG);

    while (read_length = read(file_handle, block->start + used_length,
                              allocated_length - used_length),
           read_length > 0) {
      used_length += read_length;
      if (used_length == allocated_length) {
        allocated_length += (1 << SWALLOW_REALLOC_LOG);
        block->start = (char *)xrealloc(block->start, allocated_length);
      }
    }

    if (read_length < 0)
      error(EXIT_FAILURE, errno, file_name);

    block->end = block->start + used_length;
  }

  /* Close the file, but only if it was not the standard input.  */

  if (file_handle != fileno(stdin))
    close(file_handle);
}

/* Keyword recognition and selection.  */

/*----------------------------------------------------------------------.
| For each keyword in the source text, constructs an OCCURS structure.  |
`----------------------------------------------------------------------*/

// static void find_fields_in_text(BLOCK text_buffer, char *fs, int is_regex,
//                                 int total_line_count[1],
//                                 int maximum_word_length[1],
//                                 fieldref *fields_table[1],
//                                 int number_of_fields[1]) {
//   char *cursor;         /* for scanning the source text */
//   char *scan;           /* for scanning the source text also */
//   char *line_start;     /* start of the current input line */
//   char *line_scan;      /* newlines scanned until this point */
//   int reference_length; /* length of reference in input mode */
//   int reference_max_width;
//   WORD possible_key;      /* possible key, to ease searches */
//   fieldref *field_cursor; /* current fieldref under construction */
//
//   char *context_start;      /* start of left context */
//   char *context_end;        /* end of right context */
//   char *word_start;         /* start of word */
//   char *word_end;           /* end of word */
//   char *next_context_start; /* next start of left context */
//   int row = 0;
//   int col = 0;
//
//   /* reference_length is always used within `if (input_reference)'.
//      However, GNU C diagnoses that it may be used uninitialized.  The
//      following assignment is merely to shut it up.  */
//
//   reference_length = 0;
//
//   /* Tracking where lines start is helpful for reference processing.  In
//      auto reference mode, this allows counting lines.  In input reference
//      mode, this permits finding the beginning of the references.
//
//      The first line begins with the file, skip immediately this very first
//      reference in input reference mode, to help further rejection any word
//      found inside it.  Also, unconditionally assigning these variable has
//      the happy effect of shutting up lint.  */
//
//   line_start = text_buffer.start;
//   line_scan = line_start;
//
//   /* Process the whole buffer, one line or one sentence at a time.  */
//
//   for (cursor = text_buffer.start; cursor < text_buffer.end;
//        cursor = next_context_start) {
//
//     /* `context_start' gets initialized before the processing of each
//        line, or once for the whole buffer if no end of line or sentence
//        sequence separator.  */
//
//     context_start = cursor;
//
//     /* If a end of line or end of sentence sequence is defined and
//        non-empty, `next_context_start' will be recomputed to be the end of
//        each line or sentence, before each one is processed.  If no such
//        sequence, then `next_context_start' is set at the end of the whole
//        buffer, which is then considered to be a single line or sentence.
//        This test also accounts for the case of an incomplete line or
//        sentence at the end of the buffer.  */
//
//     if (0 &&
//         (re_search(context_regex, cursor, text_buffer.end - cursor, 0,
//                    text_buffer.end - cursor, &context_regs) >= 0))
//       next_context_start = cursor + context_regs.end[0];
//
//     else
//       next_context_start = text_buffer.end;
//
//     /* Include the separator into the right context, but not any suffix
//        white space in this separator; this insures it will be seen in
//        output and will not take more space than necessary.  */
//
//     context_end = next_context_start;
//
//     /* Read and process a single input line or sentence, one word at a
//        time.  */
//
//     while (1) {
//
//       if (re_search(word_regex, cursor, context_end - cursor, 0,
//                     context_end - cursor, &word_regs) < 0)
//         break;
//       word_start = cursor + word_regs.start[0];
//       word_end = cursor + word_regs.end[0];
//
//       /* Skip right to the beginning of the found word.  */
//
//       cursor = word_start;
//
//       /* Skip any zero length word.  Just advance a single position,
//          then go fetch the next word.  */
//
//       if (word_end == word_start) {
//         cursor++;
//         continue;
//       }
//
//       /* This is a genuine, non empty word, so save it as a possible
//          key.  Then skip over it.  Also, maintain the maximum length of
//          all words read so far.  It is mandatory to take the maximum
//          length of all words in the file, without considering if they
//          are actually kept or rejected, because backward jumps at output
//          generation time may fall in *any* word.  */
//
//       possible_key.start = cursor;
//       possible_key.size = word_end - word_start;
//       cursor += possible_key.size;
//
//       if (possible_key.size > maximum_word_length[0])
//         maximum_word_length[0] = possible_key.size;
//
//       /* A non-empty word has been found.  First of all, insure
//          proper allocation of the next OCCURS, and make a pointer to
//          where it will be constructed.  */
//
//       ALLOC_NEW_OCCURS(0);
//       field_cursor = fields_table[0] + number_of_fields[0];
//
//       /* Completes the fieldref structure.  */
//
//       //      field_cursor->key = possible_key;
//       field_cursor->start = context_start - possible_key.start;
//
//       number_of_fields[0]++;
//     }
//   }
// }
