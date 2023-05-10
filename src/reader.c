#include "reader.h"

#include "err.h"
#include "mem.h"
#include "util.h"

#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <stdarg.h>

#define PROCSTATFILE  "/proc/stat"
#define PROC_LINE_LEN 4096
#define SAMPLING_FREQ 1e6
#define FAIL          (-1)

static inline bool starts_with(const char * const haystack, const char * const needle) {
   return strncmp(haystack, needle, strlen(needle)) == 0;
}

static inline void checked_sscanf(const char * const str, const char * const format, ...) {
    va_list args;
    va_start(args, format);
    if (vsscanf(str, format, args) < 0)
        fatal("vsscanf");
    va_end(args);    
}

static inline int get_data_aggregated(CpuData* cpu_data, const char * const buffer) {
    checked_sscanf(buffer, "cpu %16llu %16llu %16llu %16llu %16llu %16llu %16llu %16llu %16llu %16llu",
        &cpu_data->user, &cpu_data->nice, &cpu_data->system, &cpu_data->idle, &cpu_data->io_wait, 
        &cpu_data->irq, &cpu_data->soft_irq, &cpu_data->steal, &cpu_data->guest, &cpu_data->guest_nice
    );
    cpu_data[0].online = true;
    return 0;
}

static inline int get_data_for_core(CpuData* cpu_data, const char * const buffer, const long num_cpus) {
    CpuData temp;
    unsigned cpu_id;
    checked_sscanf(buffer, "cpu%4u %16llu %16llu %16llu %16llu %16llu %16llu %16llu %16llu %16llu %16llu",
        &cpu_id, &temp.user, &temp.nice, &temp.system, &temp.idle, &temp.io_wait, 
        &temp.irq, &temp.soft_irq, &temp.steal, &temp.guest, &temp.guest_nice
    ); 
    if (cpu_id + 1 > (unsigned)num_cpus)
        return FAIL;
    memcpy(cpu_data + cpu_id + 1, &temp, sizeof(CpuData));
    cpu_data[cpu_id + 1].online = true;
    return cpu_id + 1;
}

static inline int get_data(CpuData* cpu_data, FILE* procstat_file, const long num_cpus, const size_t line) {
    char buffer[PROC_LINE_LEN + 1];
    if (!fgets(buffer, PROC_LINE_LEN, procstat_file))
        return FAIL;
    if (!starts_with(buffer, "cpu"))
        return FAIL;
    return line == 0 
        ? get_data_aggregated(cpu_data, buffer)
        : get_data_for_core(cpu_data, buffer, num_cpus);
}

static CpuDataSample get_sample() {
    long num_cpus = sysconf(_SC_NPROCESSORS_CONF); // assume this as the upper bound for relevant lines
    if (num_cpus < 0)
        fatal("sysconf");
    
    FILE* procstat_file = fopen(PROCSTATFILE, "r");
    if (!procstat_file)
        fatal("fopen");

    CpuDataSample sample = {
        .cpu_data = checked_malloc((num_cpus + 1) * sizeof(CpuData)),
        .length = 0,
    };

    for (long i = 0; i < num_cpus + 1; ++i)
        sample.cpu_data[i].online = false;
    for (long i = 0; i < num_cpus + 1; ++i) {
        int res = get_data(sample.cpu_data, procstat_file, num_cpus, i);
        if (res == FAIL)
            break; // end of relevant lines
        sample.length = 1 + MAX(sample.length, res);
    }
    
    if (fclose(procstat_file) < 0)
        fatal("fclose");
    return sample; // don't forget to free!
}

CpuDataSample* get_samples() {
    CpuDataSample* samples = checked_malloc(NUM_SAMPLES * sizeof(CpuDataSample));
    for (size_t i = 0; i < NUM_SAMPLES; ++i) {
        samples[i] = get_sample();
        usleep(SAMPLING_FREQ / NUM_SAMPLES);
    }
    return samples; // don't forget to free!
}

void free_samples(CpuDataSample * const samples) {
    for (size_t i = 0; i < NUM_SAMPLES; ++i)
        free(samples[i].cpu_data);
    free(samples);
}
