#include "mem.h"
#include "util.h"
#include "reader.h"
#include "pthread_util.h"

#include <assert.h>
#include <signal.h>

#define READER                  0
#define NUM_WORKERS             1

volatile sig_atomic_t running = true;

static void sigterm_handler(int signum) {
    assert(signum == SIGTERM);
    fprintf(stderr, "Received SIGTERM. Shutting down...\n");
    running = false;
}

static void* reader_work(void* arg) {
    fprintf(stderr, "[Reader] starting work!\n");

    while (running) {
        CpuDataSample* samples = get_samples();
        fprintf(stderr, "[Reader] got new samples!\n");
        free_samples(samples);
    }

    fprintf(stderr, "[Reader] shutting down...\n");
    return NULL;
}

int main(void) {
#ifndef __linux__
    fatal("CUT (CPU Usage Tracker) only works on Linux!");
#else
    struct sigaction sa;
    sa.sa_handler = sigterm_handler;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = 0;
    sigaction(SIGTERM, &sa, NULL);
    
    pthread_t workers[NUM_WORKERS];
    thr_spawn(workers + READER, reader_work, NULL);

    thr_join(workers[READER], NULL);

    fprintf(stderr, "[Main] shutting down...\n");
    return 0;
#endif /*__linux__*/
}
