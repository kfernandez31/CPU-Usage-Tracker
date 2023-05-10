#pragma once

#include <stdbool.h>

#define NUM_SAMPLES 10

typedef unsigned long long cpu_time_t;

typedef struct {
    cpu_time_t user;
    cpu_time_t nice;
    cpu_time_t system;
    cpu_time_t idle;
    cpu_time_t io_wait;
    cpu_time_t irq;
    cpu_time_t soft_irq;
    cpu_time_t steal;
    cpu_time_t guest;
    cpu_time_t guest_nice;
    bool online;
} CpuData;

typedef struct {
    CpuData* cpu_data;
    long length;
} CpuDataSample;

CpuDataSample* get_samples();
void free_samples(CpuDataSample * const samples);
