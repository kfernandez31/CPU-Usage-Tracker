#include "err.h"

#include <stdio.h>
#include <errno.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <stdnoreturn.h>

noreturn void fatal(const char * const msg) {
    fprintf(stderr, "Error: %s", msg);
    if (errno)
        fprintf(stderr, ": %s.", strerror(errno));
    fprintf(stderr, "\n");
    exit(errno ? errno : EXIT_FAILURE);
}

noreturn void vfatal(const char * const msg, ...) {
    fprintf(stderr, "Error: ");

    va_list args;
    va_start(args, msg);
    vfprintf(stderr, msg, args);
    va_end(args);
    
    if (errno)
        fprintf(stderr, ": %s.", strerror(errno));
    
    fprintf(stderr, "\n");
    exit(errno ? errno : EXIT_FAILURE);
}
