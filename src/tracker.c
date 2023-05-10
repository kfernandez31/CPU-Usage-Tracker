#include "mem.h"
#include "util.h"
#include "queue.h"
#include "reader.h"
#include "analyzer.h"
#include "printer.h"
#include "logger.h"
#include "pthread_util.h"

#include <assert.h>
#include <signal.h>
#include <unistd.h>
#include <stdatomic.h>

#define READER                  0
#define ANALYZER                1
#define PRINTER                 2
#define LOGGER                  3
#define WATCHDOG                4
#define NUM_WORKERS             4
#define WATCHDOG_TIMEOUT_MICROS 2000000
#define PING_ATTEMPTS           4
#define LOCKING_TIMEOUT_NANOS   ((WATCHDOG_TIMEOUT_MICROS / 1000) / PING_ATTEMPTS) 

#define ATOMIC_PUSH_BACK(worker, worker_id, watchdog, item)            \
do {                                                                   \
    lock_and_ping(&worker->mtx, watchdog, worker_id);                  \
    queue_push_back(&worker->job_queue, item);                         \
    worker->wait = false;                                              \
    cnd_signal(&worker->cnd);                                          \
    mtx_unlock(&worker->mtx);                                          \
} while(0)

#define ORDER_TERMINATION(worker)                                      \
do {                                                                   \
    mtx_lock(&worker->mtx);                                            \
    worker->wait = false;                                              \
    cnd_signal(&worker->cnd);                                          \
    mtx_unlock(&worker->mtx);                                          \
} while(0)

#define ASYNC_LOG(level, worker_id, watchdog, logger, ...)             \
do {                                                                   \
    LogMsg* msg = new_log_msg(level, __FILE__, __LINE__, __VA_ARGS__); \
    ATOMIC_PUSH_BACK(logger, worker_id, watchdog, &msg);               \
} while(0)

typedef struct { 
    Queue job_queue;
    bool wait;
    pthread_mutex_t mtx;
    pthread_cond_t cnd;
} WorkerCtx;

typedef struct {
    _Atomic bool alive[NUM_WORKERS];
    WorkerCtx* logger;
} WatchdogCtx;

typedef struct {
    WorkerCtx self;
    WatchdogCtx* watchdog;
    WorkerCtx* logger;
    WorkerCtx* next;
} SharedWorkerCtx;

typedef SharedWorkerCtx PrinterCtx;
typedef SharedWorkerCtx AnalyzerCtx;
typedef SharedWorkerCtx LoggerCtx;

static const char * const worker_names[] = {
    "Reader",
    "Analyzer",
    "Printer",
    "Logger",
};

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

static SharedWorkerCtx* new_shared_worker_ctx(const size_t queue_item_size, WatchdogCtx * const watchdog, WorkerCtx * const logger, WorkerCtx * const next) {
    SharedWorkerCtx* ctx = checked_malloc(sizeof(*ctx));
    init_worker_ctx(&ctx->self, queue_item_size);
    ctx->watchdog = watchdog;
    ctx->watchdog->logger = ctx->logger = logger;
    ctx->next = next;
    return ctx;
}

static WatchdogCtx* new_watchdog_ctx() {
    WatchdogCtx* ctx = checked_malloc(sizeof(*ctx));
    for (size_t i = 0; i < NUM_WORKERS; ++i)
        atomic_init(ctx->alive + i, false);
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

static void destroy_logger_ctx(LoggerCtx * const ctx) {
    while (!queue_empty(&ctx->self.job_queue)) {
        LogMsg* msg = *(LogMsg**)queue_front(&ctx->self.job_queue);
        print_log_msg(msg); // print leftover logs, maybe they're relevant for all we know
        queue_pop_front(&ctx->self.job_queue);
    }
    destroy_worker_ctx(&ctx->self);
    free(ctx);
}

static void destroy_watchdog_ctx(WatchdogCtx * const ctx) {
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

static void ping_watchdog(WatchdogCtx * const watchdog, const size_t worker_id) {
    atomic_store(watchdog->alive + worker_id, true);
}

static void mark_for_death(WatchdogCtx * const watchdog, const size_t worker_id) {
    atomic_store(watchdog->alive + worker_id, false);
}

static void lock_and_ping(pthread_mutex_t * const mtx, WatchdogCtx * const watchdog, const size_t worker_id) {
    // doing timed_lock instead of lock in order not to perish accidentally 
    // because of being hanged on a mutex/condition
    do {
        ping_watchdog(watchdog, worker_id);
    } while (!mtx_timed_lock(mtx, LOCKING_TIMEOUT_NANOS));
}

static void* reader_work(void* arg) {
    WatchdogCtx* watchdog = ((AnalyzerCtx*)arg)->watchdog;
    WorkerCtx* logger     = ((AnalyzerCtx*)arg)->logger;
    WorkerCtx* analyzer   = &((AnalyzerCtx*)arg)->self;
    ASYNC_LOG(LOG_INFO, READER, watchdog, logger, "[Reader] starting work!");

    while (running) {
        ping_watchdog(watchdog, READER);
        CpuDataSample* samples = get_samples();
        ASYNC_LOG(LOG_INFO, READER, watchdog, logger, "[Reader] got new samples!");
        ATOMIC_PUSH_BACK(analyzer, READER, watchdog, &samples);
    }

    ORDER_TERMINATION(analyzer);
    ASYNC_LOG(LOG_WARN, READER, watchdog, logger, "[Reader] shutting down...");
    return NULL;
}

static void* analyzer_work(void* arg) {
    WorkerCtx* self       = &((AnalyzerCtx*)arg)->self;
    WatchdogCtx* watchdog = ((AnalyzerCtx*)arg)->watchdog;
    WorkerCtx* logger     = ((AnalyzerCtx*)arg)->logger;
    WorkerCtx* printer    = ((AnalyzerCtx*)arg)->next;
    ASYNC_LOG(LOG_INFO, ANALYZER, watchdog, logger, "[Analyzer] starting work!");

    while (running) {
        lock_and_ping(&self->mtx, watchdog, ANALYZER);
        if (!should_continue_work(self))
            break;
        ASYNC_LOG(LOG_INFO, ANALYZER, watchdog, logger, "[Analyzer] woke up, resuming work");
        assert(!queue_empty(&self->job_queue));
        CpuDataSample* samples = *(CpuDataSample**)queue_front(&self->job_queue);
        queue_pop_front(&self->job_queue);
        mtx_unlock(&self->mtx);

        CpuUsage usage = get_usage(samples);
        ASYNC_LOG(LOG_INFO, ANALYZER, watchdog, logger, "[Analyzer] gathered new usage info");
        ATOMIC_PUSH_BACK(printer, ANALYZER, watchdog, &usage);
    }

    ORDER_TERMINATION(printer);
    ASYNC_LOG(LOG_WARN, ANALYZER, watchdog, logger, "[Analyzer] shutting down...");
    return NULL;
}

static void* printer_work(void* arg) {
    WorkerCtx* self       = &((PrinterCtx*)arg)->self;
    WatchdogCtx* watchdog = ((PrinterCtx*)arg)->watchdog;
    WorkerCtx* logger     = ((PrinterCtx*)arg)->logger;
    ASYNC_LOG(LOG_INFO, PRINTER, watchdog, logger, "[Printer] starting work!");

    while (running) {
        lock_and_ping(&self->mtx, watchdog, PRINTER);
        if (!should_continue_work(self))
            break;
        ASYNC_LOG(LOG_INFO, PRINTER, watchdog, logger, "[Printer] woke up, resuming work");
        assert(!queue_empty(&self->job_queue));
        CpuUsage usage = *(CpuUsage*)queue_front(&self->job_queue);
        queue_pop_front(&self->job_queue);
        mtx_unlock(&self->mtx);
        print_usage(usage);
        ASYNC_LOG(LOG_INFO, PRINTER, watchdog, logger, "[Printer] printed usage info");
    }

    ASYNC_LOG(LOG_WARN, PRINTER, watchdog, logger, "[Printer] shutting down...");
    ORDER_TERMINATION(logger); // let the printer do this as the last worker in the chain
    return NULL;
}

// yes, the workers' logs are not perfect (they're kinda noisy/spammy)
// but it's for a good reason - 
// for the logger to not be accidentally marked as dead,
// the other workers need to keep giving him work to do
static void* logger_work(void* arg) {
    WorkerCtx* self       = &((LoggerCtx*)arg)->self;
    WatchdogCtx* watchdog = ((LoggerCtx*)arg)->watchdog;
    log_info("[Logger] starting work!");

    while (running) {
        lock_and_ping(&self->mtx, watchdog, LOGGER);
        if (!should_continue_work(self))
            break;
        log_info("[Logger] woke up, resuming work"); // disregard the queue's order
        assert(!queue_empty(&self->job_queue));
        LogMsg* msg = *(LogMsg**)queue_front(&self->job_queue);
        queue_pop_front(&self->job_queue);
        mtx_unlock(&self->mtx);

        print_log_msg(msg);
    }

    log_warn("[Logger] shutting down...");
    return NULL;
}

// not using the logger here to avoid any data races
// and a dependency upon a possibly dead worker (thus a dedadlock)
static void* watchdog_work(void* arg) {
    WatchdogCtx* self = (WatchdogCtx*)arg;

    while (running) {
        usleep(WATCHDOG_TIMEOUT_MICROS);
        for (size_t i = 0; i < NUM_WORKERS; ++i) {
            if (running && !atomic_load(self->alive + i)) {
                checked_fprintf(stderr, "[Watchdog] worker #%zu (%s) died!\n", i, worker_names[i]);
                exit(EXIT_FAILURE);
            }
            mark_for_death(self, i);
        }
        checked_fprintf(stderr, "[Watchdog] all workers are alive\n");
    }

    checked_fprintf(stderr, "[Watchdog] shutting down...\n");
    return NULL;
}

int main(void) {
#ifndef __linux__
    fatal("CUT (CPU Usage Tracker) only works on Linux!");
#else
    logger_init(true);

    struct sigaction sa;
    sa.sa_handler = sigterm_handler;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = 0;
    sigaction(SIGTERM, &sa, NULL);

    WatchdogCtx* watchdog_ctx = new_watchdog_ctx();
    LoggerCtx* logger_ctx     = new_shared_worker_ctx(sizeof(LogMsg*), watchdog_ctx, NULL, NULL);
    PrinterCtx* printer_ctx   = new_shared_worker_ctx(sizeof(CpuUsage), watchdog_ctx, &logger_ctx->self, NULL);
    AnalyzerCtx* analyzer_ctx = new_shared_worker_ctx(sizeof(CpuDataSample*), watchdog_ctx, &logger_ctx->self, &printer_ctx->self);
    
    pthread_t workers[NUM_WORKERS + 1];
    thr_spawn(workers + LOGGER, logger_work, logger_ctx);
    thr_spawn(workers + PRINTER, printer_work, printer_ctx);
    thr_spawn(workers + ANALYZER, analyzer_work, analyzer_ctx);
    thr_spawn(workers + READER, reader_work, analyzer_ctx);
    thr_spawn(workers + WATCHDOG, watchdog_work, watchdog_ctx);

    thr_join(workers[READER], NULL);
    thr_join(workers[ANALYZER], NULL);
    thr_join(workers[PRINTER], NULL);
    thr_join(workers[LOGGER], NULL);
    thr_join(workers[WATCHDOG], NULL);

    // the workers' queues might not be empty at this point, so we need to drain them
    // the following lines will do just that, and a bit more
    destroy_logger_ctx(logger_ctx);
    destroy_printer_ctx(printer_ctx);
    destroy_analyzer_ctx(analyzer_ctx);
    destroy_watchdog_ctx(watchdog_ctx);

    fprintf(stderr, "[Main] shutting down...\n");
    logger_destroy();
    return 0;
#endif /*__linux__*/
}
