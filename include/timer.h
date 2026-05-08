#ifndef TIMER_H
#define TIMER_H

#include <time.h>

typedef struct {
    struct timespec start;
    struct timespec end;
} Timer;

void timer_start(Timer* t)
{
    clock_gettime(CLOCK_MONOTONIC, &t->start);
}
double timer_stop(Timer* t)
{
    clock_gettime(CLOCK_MONOTONIC, &t->end);

    double us = (t->end.tv_sec - t->start.tv_sec) * 1e6;
    us += (t->end.tv_nsec - t->start.tv_nsec) / 1e3;

    return us;
}
#endif