#ifndef __UTILS_H__
#define __UTILS_H__

#include <stdint.h>
#include <time.h>
#include <sys/types.h>
#include <sys/stat.h>

/** nanosec */
static inline uint64_t nowNs(void)
{
    struct timespec ts;

    clock_gettime(CLOCK_MONOTONIC, &ts);

    return (ts.tv_sec * INT64_C(1000000000)) + (ts.tv_nsec);
}

/** millsec */
static inline uint64_t nowMs(void)
{
    return nowNs() / INT64_C(1000000);
}

/** microsec */
static inline uint64_t nowUs(void)
{
    return nowNs() / INT64_C(1000);
}

#endif
