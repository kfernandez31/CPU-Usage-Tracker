#ifdef NDEBUG
#undef NDEBUG
#endif

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

typedef struct {
    const char * const name;
    bool (*run)(void);
} test_t;

static bool test_true() {
    return true;
}

static const test_t tests[] = {
    TEST(test_true),
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
