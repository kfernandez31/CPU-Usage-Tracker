#pragma once

#include <sys/types.h>
#include <stdio.h>

#define MAX(a,b) ((a) > (b) ? (a) : (b))

size_t checked_snprintf(char * const buffer, const size_t max_len, const char * const format, ...);
void checked_fprintf(FILE * const stream, const char * const format, ...);
