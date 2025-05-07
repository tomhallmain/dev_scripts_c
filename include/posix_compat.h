#ifndef POSIX_COMPAT_H
#define POSIX_COMPAT_H

// #if defined(_WIN32) || defined(__MINGW32__)
#include <stdio.h>
#include <errno.h>
#include <io.h>
#include <fcntl.h>
#include <process.h>

// POSIX compatibility functions
int mkstemp(char *template);
char *stpcpy(char *dest, const char *src);
char *strdup(const char *s);
int fileno(FILE *stream);
FILE *fdopen(int fd, const char *mode);
char *index(const char *s, int c);

// #endif // _WIN32 || __MINGW32__

#endif // POSIX_COMPAT_H 