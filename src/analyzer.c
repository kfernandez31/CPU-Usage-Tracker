#include "analyzer.h"

#include "mem.h"
#include "util.h"

#include <stddef.h>
#include <assert.h>

static cpu_usage_t get_usage_for_core(const unsigned core, const CpuDataSample * const samples) {
    assert(NUM_SAMPLES > 1);
    for (size_t i = 0; i < NUM_SAMPLES; ++i)
        if (samples[i].length - 1 < core || !samples[i].cpu_data[core].online)
            return UNKNOWN_USAGE;

    cpu_time_t idle[NUM_SAMPLES], non_idle[NUM_SAMPLES], total[NUM_SAMPLES];

    for (size_t i = 0; i < NUM_SAMPLES; ++i) {
        CpuData* data = &samples[i].cpu_data[core];
        idle[i]     = data->idle + data->io_wait;
        non_idle[i] = data->user + data->nice + data->system + data->irq + data->soft_irq + data->steal;
        total[i]    = idle[i] + non_idle[i];
    }

    cpu_usage_t avg_usage = 0;
    for (size_t i = 1; i < NUM_SAMPLES; ++i) {
        cpu_time_t delta_total = total[i] - total[i - 1];
        cpu_time_t delta_idle  = idle[i]  - idle[i - 1];
        assert(delta_total >= delta_idle);
        assert(delta_total > 0);
        cpu_usage_t usage = (cpu_usage_t)(delta_total - delta_idle) / (cpu_usage_t)delta_total;
        avg_usage += usage / (NUM_SAMPLES - 1);
    }
    return avg_usage * 100.0f;
}

CpuUsage get_usage(CpuDataSample * const samples) {
    long max_len = 0;
    
    for (long i = 0; i < NUM_SAMPLES; ++i)
        max_len = MAX(max_len, samples[i].length);
    
    CpuUsage usage = {
        .usage  = checked_malloc(max_len * sizeof(cpu_usage_t)),
        .length = max_len,
    };

    for (long i = 0; i < usage.length; ++i)
        usage.usage[i] = get_usage_for_core(i, samples);

    free_samples(samples);
    return usage; // don't forget to free!
}

void free_usage(CpuUsage usage) {
    free(usage.usage);    
}
