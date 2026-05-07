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

long timer_stop(Timer* t)
{
    clock_gettime(CLOCK_MONOTONIC, &t->end);
    
    long ms = (t->end.tv_sec - t->start.tv_sec) * 1000;
    ms += (t->end.tv_nsec - t->start.tv_nsec) / 1000000;
    
    return ms;
}

#endif