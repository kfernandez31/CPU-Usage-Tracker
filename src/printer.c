#include "printer.h"

#include "err.h"
#include "util.h"

#include <unistd.h>
#include <stdio.h>

#define CPU_ID_MAX_DECIMAL_DIGITS 6
#define INCOMPLETE_ROW_LEN        (sizeof("cpu : ###.##%\n"))
#define ROW_LEN                   (INCOMPLETE_ROW_LEN + CPU_ID_MAX_DECIMAL_DIGITS)

static const char* ansi_clear = "\x1b[2J";

static void clear_screen() {
    printf("%s", ansi_clear);
    fflush(stdout);
}

void print_usage(CpuUsage usage) {
    char buffer[usage.length * ROW_LEN];
    size_t buf_pos = 0;
    for (long cpu = 0; cpu < usage.length; ++cpu) {
        size_t nleft = ROW_LEN;
        size_t nprinted = cpu == 0
            ? checked_snprintf(buffer + buf_pos, nleft, "total: ")
            : checked_snprintf(buffer + buf_pos, nleft, "cpu %ld: ", cpu - 1);
        nleft   -= nprinted;
        buf_pos += nprinted;
        buf_pos += usage.usage[cpu] == UNKNOWN_USAGE
            ? checked_snprintf(buffer + buf_pos, nleft, "UNKNOWN\n")
            : checked_snprintf(buffer + buf_pos, nleft, "%.2f%%\n", usage.usage[cpu]);
    }
    clear_screen();
    if (write(STDOUT_FILENO, buffer, buf_pos) < 0)
        fatal("write");
    free_usage(usage);
}
