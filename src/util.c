#include "util.h"

#include "err.h"

#include <stdio.h>
#include <stdarg.h>

size_t checked_snprintf(char * const buffer, const size_t max_len, const char * const format, ...) {
    va_list args;
    va_start(args, format);
    ssize_t nprinted = vsnprintf(buffer, max_len, format, args);
    va_end(args);

    if (nprinted < 0)
        fatal("vsnprintf");
    return (size_t)nprinted;
}

void checked_fprintf(FILE * const stream, const char * const format, ...) {
    va_list args;
    va_start(args, format);
    if (vfprintf(stream, format, args) < 0)
        fatal("vfprintf");
    va_end(args);
}
