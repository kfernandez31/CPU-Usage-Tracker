#pragma once

#include "err.h"

#include <pthread.h>
#include <stdbool.h>
#include <time.h>
#include <errno.h>

#define PTHREAD_CHECK(func, x) \
do {                           \
    int res = (x);             \
    if (res != 0)              \
        fatal(#func);          \
} while (0)

typedef void* (*pthread_routine_t) (void*);

static inline void mtx_init(pthread_mutex_t * const mtx) {
    PTHREAD_CHECK(pthread_mutex_init, pthread_mutex_init(mtx, NULL));
}

static inline void mtx_destroy(pthread_mutex_t * const mtx) {
    PTHREAD_CHECK(pthread_mutex_init, pthread_mutex_destroy(mtx));
}

static inline void mtx_lock(pthread_mutex_t * const mtx) {
    PTHREAD_CHECK(pthread_mutex_lock, pthread_mutex_lock(mtx));
}

static inline bool mtx_timed_lock(pthread_mutex_t * const mtx, const size_t nanos) {
    struct timespec timeoutTime;
    clock_gettime(CLOCK_REALTIME, &timeoutTime);
    timeoutTime.tv_nsec += nanos;

    int ret = pthread_mutex_timedlock(mtx, &timeoutTime);
    if (ret == ETIMEDOUT)
        return false;
    if (ret == 0)
        return true;
    fatal("pthread_mutex_timedlock");
}

static inline void mtx_unlock(pthread_mutex_t * const mtx) {
    PTHREAD_CHECK(pthread_mutex_unlock, pthread_mutex_unlock(mtx));
}

static inline void cnd_init(pthread_cond_t * const cnd) {
    PTHREAD_CHECK(pthread_cond_init, pthread_cond_init(cnd, NULL));
}

static inline void cnd_destroy(pthread_cond_t * const cnd) {
    PTHREAD_CHECK(pthread_cond_destroy, pthread_cond_destroy(cnd));
}

static inline void cnd_signal(pthread_cond_t * const cnd) {
    PTHREAD_CHECK(pthread_cond_signal, pthread_cond_signal(cnd));
}

static inline void cnd_wait(pthread_cond_t * const cnd, pthread_mutex_t * const mtx) {
    PTHREAD_CHECK(pthread_cond_wait, pthread_cond_wait(cnd, mtx));
}

static inline void thr_spawn(pthread_t * const handle, pthread_routine_t routine, void * const ctx) {
    PTHREAD_CHECK(pthread_create, pthread_create(handle, NULL, routine, ctx));
}

static inline void thr_join(const pthread_t handle, void** ret) {
    PTHREAD_CHECK(pthread_join, pthread_join(handle, ret));   
}
