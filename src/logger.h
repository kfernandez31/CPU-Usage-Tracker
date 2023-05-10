#pragma once

#include <stdbool.h>
#include <stddef.h>

typedef enum { 
    LOG_TRACE, 
    LOG_DEBUG, 
    LOG_INFO, 
    LOG_WARN, 
    LOG_ERROR, 
    LOG_FATAL 
} log_level_t;

typedef struct {
    struct tm* time;
    log_level_t level;
    size_t line;
    const char* file;
    size_t length;
    char contents[]; // leaving this as a FAM guarantees one allocation. Pretty cool, right?
} LogMsg;

#define log_trace(...) log_impl(LOG_TRACE, __FILE__, __LINE__, __VA_ARGS__)
#define log_debug(...) log_impl(LOG_DEBUG, __FILE__, __LINE__, __VA_ARGS__)
#define log_info(...)  log_impl(LOG_INFO,  __FILE__, __LINE__, __VA_ARGS__)
#define log_warn(...)  log_impl(LOG_WARN,  __FILE__, __LINE__, __VA_ARGS__)
#define log_error(...) log_impl(LOG_ERROR, __FILE__, __LINE__, __VA_ARGS__)
#define log_fatal(...) log_impl(LOG_FATAL, __FILE__, __LINE__, __VA_ARGS__)

#define log_impl(level, file, line, ...)                       \
do {                                                           \
    LogMsg* msg = new_log_msg(level, file, line, __VA_ARGS__); \
    print_log_msg(msg);                                        \
} while(0)

LogMsg* new_log_msg(const log_level_t level, const char * const file, const size_t line, const char * const fmt, ...);
void free_log_msg(LogMsg* msg);
void print_log_msg(LogMsg* msg);
void logger_init(const bool log_to_file);
void logger_destroy();
