#include "mem.h"
#include "util.h"
#include "queue.h"
#include "reader.h"
#include "analyzer.h"
#include "printer.h"
#include "pthread_util.h"

#include <assert.h>
#include <signal.h>

#define READER                  0
#define ANALYZER                1
#define PRINTER                 2
#define NUM_WORKERS             3

#define ATOMIC_PUSH_BACK(worker, item)                                 \
do {                                                                   \
    mtx_lock(&worker->mtx);                                            \
    queue_push_back(&worker->job_queue, item);                         \
    worker->wait = false;                                              \
    cnd_signal(&worker->cnd);                                          \
    mtx_unlock(&worker->mtx);                                          \
} while(0)

#define ORDER_TERMINATION(worker)                                      \
do                                                                     \
{                                                                      \
    mtx_lock(&worker->mtx);                                            \
    worker->wait = false;                                              \
    cnd_signal(&worker->cnd);                                          \
    mtx_unlock(&worker->mtx);                                          \
} while(0)

typedef struct { 
    Queue job_queue;
    bool wait;
    pthread_mutex_t mtx;
    pthread_cond_t cnd;
} WorkerCtx;

typedef struct {
    WorkerCtx self;
    WorkerCtx* next;
} SharedWorkerCtx;

typedef SharedWorkerCtx PrinterCtx;
typedef SharedWorkerCtx AnalyzerCtx;

volatile sig_atomic_t running = true;

static void sigterm_handler(int signum) {
    assert(signum == SIGTERM);
    fprintf(stderr, "Received SIGTERM. Shutting down...\n");
    running = false;
}

static void init_worker_ctx(WorkerCtx * const ctx, const size_t queue_item_size) {
    queue_init(&ctx->job_queue, queue_item_size);
    mtx_init(&ctx->mtx);
    cnd_init(&ctx->cnd);
    ctx->wait = true;
}

static void destroy_worker_ctx(WorkerCtx * const ctx) {
    queue_destroy(&ctx->job_queue);
    mtx_destroy(&ctx->mtx);
    cnd_destroy(&ctx->cnd);
}

static SharedWorkerCtx* new_shared_worker_ctx(const size_t queue_item_size, WorkerCtx * const next) {
    SharedWorkerCtx* ctx = checked_malloc(sizeof(*ctx));
    init_worker_ctx(&ctx->self, queue_item_size);
    ctx->next = next;
    return ctx;
}

static void destroy_analyzer_ctx(AnalyzerCtx * const ctx) {
    while (!queue_empty(&ctx->self.job_queue)) {
        CpuDataSample* samples = *(CpuDataSample**)queue_front(&ctx->self.job_queue);
        free_samples(samples);
        queue_pop_front(&ctx->self.job_queue);
    }
    destroy_worker_ctx(&ctx->self);
    free(ctx);
}

static void destroy_printer_ctx(PrinterCtx * const ctx) {
    while (!queue_empty(&ctx->self.job_queue)) {
        CpuUsage usage = *(CpuUsage*)queue_front(&ctx->self.job_queue);
        free_usage(usage);
        queue_pop_front(&ctx->self.job_queue);
    }
    destroy_worker_ctx(&ctx->self);
    free(ctx);
}

static bool should_continue_work(WorkerCtx * const self) {
    if (queue_empty(&self->job_queue))
        self->wait = true;
    while (running && self->wait == true)
        cnd_wait(&self->cnd, &self->mtx);
    if (!running) {
        mtx_unlock(&self->mtx);
        return false;
    }
    return true;
}

static void* reader_work(void* arg) {
    WorkerCtx* analyzer = &((AnalyzerCtx*)arg)->self;
    fprintf(stderr, "[Reader] starting work!\n");

    while (running) {
        CpuDataSample* samples = get_samples();
        fprintf(stderr, "[Reader] got new samples!\n");
        ATOMIC_PUSH_BACK(analyzer, &samples);
    }

    ORDER_TERMINATION(analyzer);
    fprintf(stderr, "[Reader] shutting down...\n");
    return NULL;
}

static void* analyzer_work(void* arg) {
    WorkerCtx* self    = &((AnalyzerCtx*)arg)->self;
    WorkerCtx* printer = ((AnalyzerCtx*)arg)->next;
    fprintf(stderr, "[Analyzer] starting work!\n");

    while (running) {
        mtx_lock(&self->mtx);
        if (!should_continue_work(self))
            break;
        fprintf(stderr, "[Analyzer] woke up, resuming work\n");
        assert(!queue_empty(&self->job_queue));
        CpuDataSample* samples = *(CpuDataSample**)queue_front(&self->job_queue);
        queue_pop_front(&self->job_queue);
        mtx_unlock(&self->mtx);

        CpuUsage usage = get_usage(samples);
        fprintf(stderr, "[Analyzer] gathered new usage info\n");
        ATOMIC_PUSH_BACK(printer, &usage);
    }

    ORDER_TERMINATION(printer);
    fprintf(stderr, "[Analyzer] shutting down...\n");
    return NULL;
}

static void* printer_work(void* arg) {
    WorkerCtx* self = &((PrinterCtx*)arg)->self;
    fprintf(stderr, "[Printer] starting work!\n");

    while (running) {
        mtx_lock(&self->mtx);
        if (!should_continue_work(self))
            break;
        fprintf(stderr, "[Printer] woke up, resuming work\n");
        assert(!queue_empty(&self->job_queue));
        CpuUsage usage = *(CpuUsage*)queue_front(&self->job_queue);
        queue_pop_front(&self->job_queue);
        mtx_unlock(&self->mtx);
        print_usage(usage);
        fprintf(stderr, "[Printer] printed usage info\n");
    }

    fprintf(stderr, "[Printer] shutting down...\n");
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

    PrinterCtx* printer_ctx   = new_shared_worker_ctx(sizeof(CpuUsage), NULL);
    AnalyzerCtx* analyzer_ctx = new_shared_worker_ctx(sizeof(CpuDataSample*), &printer_ctx->self);
    
    pthread_t workers[NUM_WORKERS];
    thr_spawn(workers + PRINTER, printer_work, printer_ctx);
    thr_spawn(workers + ANALYZER, analyzer_work, analyzer_ctx);
    thr_spawn(workers + READER, reader_work, analyzer_ctx);

    thr_join(workers[READER], NULL);
    thr_join(workers[ANALYZER], NULL);
    thr_join(workers[PRINTER], NULL);

    // the workers' queues might not be empty at this point, so we need to drain them
    // the following lines will do just that, and a bit more
    destroy_printer_ctx(printer_ctx);
    destroy_analyzer_ctx(analyzer_ctx);

    fprintf(stderr, "[Main] shutting down...\n");
    return 0;
#endif /*__linux__*/
}
