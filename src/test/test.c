#ifdef NDEBUG
#undef NDEBUG
#endif

#include "../mem.h"
#include "../queue.h"
#include "../reader.h"
#include "../analyzer.h"
#include "../printer.h"

#include <string.h>
#include <stdlib.h>
#include <stdbool.h>
#include <stdio.h>

#define SIZE(x) (sizeof (x) / sizeof (x)[0])
#define TEST(t) {#t, t}
#define CHECK(x)                             \
do {                                         \
    if (!(x)) {                              \
        fprintf(stderr, "%s was false", #x); \
        return false;                        \
    }                                        \
} while (0)

#define BS_DATA_LEN 512

typedef struct {
    const char * const name;
    bool (*run)(void);
} test_t;

typedef struct {
    char data[BS_DATA_LEN + 1];
} big_struct;

static char* repeat(const char c, const size_t n) {
    char* str = checked_malloc(n + 1);
    memset(str, c, n);
    str[n] = '\0';
    return str;
}

static bool test_queue_small_items_push_then_pop() {
    const size_t nreps = 100000;
    Queue q;
    queue_init(&q, sizeof(int));
    CHECK(queue_empty(&q));

    for (size_t i = 0; i < nreps; ++i) {
        queue_push_back(&q, &i);
        int* front = queue_front(&q);
        CHECK(0 == *front);
    }

    for (size_t i = 0; i < nreps; ++i) {
        int* front = queue_front(&q);
        CHECK(i == *front);
        queue_pop_front(&q);
    }

    CHECK(queue_empty(&q));
    queue_destroy(&q);
    return true;
}

static bool test_queue_big_items_push_then_pop() {
    const int nreps = 26;
    Queue q;
    queue_init(&q, sizeof(big_struct));
    CHECK(queue_empty(&q));

    char* expected_front = repeat('a', BS_DATA_LEN);
    for (int i = 0; i < nreps; ++i) {
        big_struct bs;
        memset(bs.data, 'a' + i, BS_DATA_LEN);
        bs.data[BS_DATA_LEN] = 0;

        queue_push_back(&q, &bs);

        big_struct* front = queue_front(&q);
        CHECK(0 == strncmp(expected_front, front->data, BS_DATA_LEN));
    }
    free(expected_front);

    for (int i = 0; i < nreps; ++i) {
        big_struct* front = queue_front(&q);

        expected_front = repeat('a' + i, BS_DATA_LEN);
        CHECK(0 == strncmp(expected_front, front->data, BS_DATA_LEN));
        free(expected_front);

        queue_pop_front(&q);
    }
    CHECK(queue_empty(&q));
    queue_destroy(&q);
    return true;
}

//TODO: a test for pushes and pops intertwined

// Yes, this is an integration test without unit tests for the underlying functions. 
// But just how are you going to test whether get_samples returns somethign of sense? 
// You'd just print it and look at it. Same can be said for get_usage, 
// so might as well combine them all together.
static bool test_get_samples_get_data_print_data() {
#ifdef __linux__
    CpuDataSample* samples = get_samples();
    CpuUsage usage = get_usage(samples);
    print_usage(usage);
#endif /* __linux__ */
    return true;
}

static const test_t tests[] = {
    TEST(test_queue_small_items_push_then_pop),
    TEST(test_queue_big_items_push_then_pop),
    TEST(test_get_samples_get_data_print_data),
};

int main(void) {
    bool OK = true;
    for (size_t i = 0; i < SIZE(tests); ++i) {
        bool run = tests[i].run();
        if (run)
            fprintf(stderr, "test %s: OK\n", tests[i].name);
        else
            fprintf(stderr, "test %s: FAIL\n", tests[i].name);
        OK &= run;
    }

    if (OK)
        fprintf(stderr, "All tests passed\n");
    else
        fprintf(stderr, "Tests failed\n");
    return 0;
}
