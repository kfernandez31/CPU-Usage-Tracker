#pragma once

#include "reader.h"

#define UNKNOWN_USAGE (-1.0f)

typedef float cpu_usage_t;

typedef struct {
    cpu_usage_t* usage;
    long length;
} CpuUsage;

CpuUsage get_usage(CpuDataSample * const samples);
void free_usage(CpuUsage usage);
