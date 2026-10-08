#ifndef DEVICES_TIMER_H
#define DEVICES_TIMER_H

#include <round.h>
#include <stdint.h>
#include "threads/synch.h"

/* Number of timer interrupts per second. */
#define TIMER_FREQ 100

/* Constains records of sleeping threads sorted by expirty_ticks in ascending
   order. Only access this list with interrupts off because it's accessed by 
   timer interrupt handler */
static struct list sleep_list;

/* Individual record of each sleeping thread in sleep_list */
struct sleep_record {
  int64_t expiry_tick;    /* Tick at which the thread should wake up */
  struct list_elem elem;  /* List element in sleep_list */
  struct semaphore sema;  /* Points at semaphore handling sleeping thread */
};

void timer_init (void);
void timer_calibrate (void);

int64_t timer_ticks (void);
int64_t timer_elapsed (int64_t);

/* Sleep and yield the CPU to other threads. */
void timer_sleep (int64_t ticks);
void timer_msleep (int64_t milliseconds);
void timer_usleep (int64_t microseconds);
void timer_nsleep (int64_t nanoseconds);

/* Busy waits. */
void timer_mdelay (int64_t milliseconds);
void timer_udelay (int64_t microseconds);
void timer_ndelay (int64_t nanoseconds);

void timer_print_stats (void);

#endif /* devices/timer.h */
