#include "../include/posix_compat.h"
#include <stdio.h>
#include <sys/stat.h>

#ifdef _WIN32
int mkstemp(char *template) {
    char *tmp = _mktemp(template);
    if (!tmp) {
        errno = EINVAL;
        return -1;
    }
    int fd = _open(tmp, _O_CREAT | _O_RDWR | _O_BINARY, _S_IREAD | _S_IWRITE);
    if (fd == -1) {
        errno = EACCES;
    }
    return fd;
}

char *stpcpy(char *dest, const char *src) {
    while ((*dest++ = *src++) != '\0');
    return dest - 1;
}

char *strdup(const char *s) {
    if (!s) {
        errno = EINVAL;
        return NULL;
    }
    size_t len = strlen(s) + 1;
    char *new = malloc(len);
    if (new) {
        memcpy(new, s, len);
    } else {
        errno = ENOMEM;
    }
    return new;
}

int fileno(FILE *stream) {
    if (!stream) {
        errno = EINVAL;
        return -1;
    }
    return _fileno(stream);
}

FILE *fdopen(int fd, const char *mode) {
    if (fd < 0 || !mode) {
        errno = EINVAL;
        return NULL;
    }
    return _fdopen(fd, mode);
}

char *index(const char *s, int c) {
    if (!s) {
        errno = EINVAL;
        return NULL;
    }
    return strchr(s, c);
}
#endif // _WIN32 