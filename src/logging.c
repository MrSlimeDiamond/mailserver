#include "logging.h"

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

// 1 is true, 0 is false
#define DEBUG 1

const char *get_timestamp(void) {
    static char buffer[100];
    time_t now = time(NULL);
    struct tm t;

#ifdef _WIN32
    localtime_s(&t, &now);
#else
    localtime_r(&now, &t);
#endif

    strftime(buffer, sizeof(buffer), "%d-%m-%Y %H:%M:%S", &t);
    return buffer;
}

static void vdo_log(const char *level, const char *fmt, va_list args) {
    va_list args_copy;
    va_copy(args_copy, args);

    int len = vsnprintf(NULL, 0, fmt, args_copy);
    va_end(args_copy);

    char *buffer = malloc(len + 1);
    if (!buffer) return;

    vsnprintf(buffer, len + 1, fmt, args);

    printf("%s [%s] %s\n", get_timestamp(), level, buffer);

    free(buffer);
}

void do_log(char *level, char *fmt, ...) {
    va_list args;
    va_start(args, fmt);
    vdo_log(level, fmt, args);
    va_end(args);
}

void log_info(char *fmt, ...) {
    va_list args;
    va_start(args, fmt);
    vdo_log("INFO", fmt, args);
    va_end(args);
}

void log_warn(char *fmt, ...) {
    va_list args;
    va_start(args, fmt);
    vdo_log("WARN", fmt, args);
    va_end(args);
}

void log_error(char *fmt, ...) {
    va_list args;
    va_start(args, fmt);
    vdo_log("ERROR", fmt, args);
    va_end(args);
}

void log_debug(char *fmt, ...) {
    if (DEBUG) {
        va_list args;
        va_start(args, fmt);
        vdo_log("DEBUG", fmt, args);
        va_end(args);
    }
}