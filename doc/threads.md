            +----------------------+
            |    COMP50007/50012   |
            |  TASK 1: SCHEDULING  |
            |    DESIGN DOCUMENT   |
            +----------------------+

---- GROUP ----

>> Fill in the names and email addresses of your group members.

Shi Hao Ng <shi-hao.ng25@imperial.ac.uk>
Shiven Tayal <shiven.tayal25@imperial.ac.uk>
Suhaan Mobhani <suhaan.mobhani25@imperial.ac.uk>
Sun Young Li <sun.li@imperial.ac.uk>

---- PRELIMINARIES ----

>> If you have any preliminary comments on your submission, or notes for the
>> markers, please give them here.

>> Please cite any offline or online sources you consulted while preparing your
>> submission, other than the PintOS documentation, course text, lecture notes
>> and course staff.

             PRIORITY SCHEDULING
             ===================

---- DATA STRUCTURES ----

>> A1: Copy here the declaration of each new or changed `struct' or `struct'
>> member, global or static variable, `typedef', or enumeration.
>> Identify the purpose of each in roughly 25 words.

---- ALGORITHMS ----

>> A2: How do you ensure that the highest priority waiting thread wakes up first
>> for a (i) semaphore, (ii) lock, or (iii) condition variable?

>> A3: Describe the sequence of events when a call to lock_acquire() causes a
>> priority donation. How is nested donation handled?

>> A4: Describe the sequence of events when lock_release() is called on a lock
>> that a higher-priority thread is waiting for.

---- SYNCHRONISATION ----

>> A5: How do you avoid a race condition in thread_set_priority() when a thread
>> needs to recompute its effective priority, but the donated priorities
>> potentially change during the computation?
>> Can you use a lock to avoid the race?

---- RATIONALE ----

>> A6: Why did you choose this design?
>> In what ways is it superior to another design you considered?

              ADVANCED SCHEDULER
              ============
              ======

---- DATA STRUCTURES ----

>> B1: Copy here the declaration of each new or changed `struct' or `struct'
>> member, global or static variable, `typedef', or enumeration.
>> Identify the purpose of each in roughly 25 words.

`int nice;` in `struct thread` (thread.h)
   To represent the niceness of each thread to other threads. Only used when
   BSD scheduler is used.

`fixed_t recent_cpu;` in `struct thread` (thread.h)
   To represent the number of CPU-time in timer ticks which each thread
   has recently used. Only used when BSD scheduler is used.

`static fixed_t load_avg;` (thread.c)
   To represent the system's current load_avg value, which is updated once
   per TIMER_FREQ timer ticks for the BSD scheduler. Only used when the BSD
   scheduler is used.

`typedef struct { int32_t raw; } fixed_t;` (fixed-point.h)
   A signed 17.14 fixed-point number used for `recent_cpu` and `load_avg`,
   since the PintOS kernel has no floating-point support. Wrapping the raw
   value in a struct makes it a distinct type, so the compiler rejects
   accidentally mixing it with plain ints.

---- ALGORITHMS ----

>> B2: What steps are taken to minimise the amount of time spent in the timer
>> interrupt handler?

Once every `TIMER_FREQ` ticks, `calculate_load_avg ()` runs first, then
`calculate_recent_cpu_and_priorities ()` walks `all_list` once, and for each
thread `update_thread_recent_cpu_and_priority ()` updates its `recent_cpu` and
then its priority. Doing both in one pass is safe because each thread's new
values depend only on its own fields and the global `load_avg`, and it avoids
the second walk of `all_list` that separate recent_cpu and priority passes
would need.

On the contrary, in the context of every 4 ticks, we only run a targeted
`update_thread_priority (t, NULL);` which only recalculates priority for the
current running thread. This is because between seconds, only the running
thread's `recent_cpu` changes, so other threads' priorities cannot change.
Hence, by ensuring that we only recalculate the priority for the least amount of
 threads needed (all threads every TIMER_FREQ, only running thread every
 PRIORITY_PERIOD), we minimise amount of CPU time spent in timer interrupt
 handler in `thread_tick()`.

Once every second, `calculate_recent_cpu_and_priorities ();` is called once. We
minimise the time `calculate_recent_cpu_and_priorities ()` spend in timer
interrupt handler by only calculating `load_avg_coeff` once, before passing it
into `thread_foreach`, so this calculation is O(1) per second. This was more
efficient than our previous design which was calculating `load_avg_coeff` again
and again within `update_thread_recent_cpu_and_priority ()` for N threads time,
which is a lot more inefficient at O(N) time complexity.

---- RATIONALE ----

>> B3: Briefly critique your design, pointing out advantages and disadvantages
>> in your design choices.

ADVANTAGE:
- The time spent in timer interrupt handler within `thread_tick` for
  incrementing current thread's `recent_cpu` every tick takes O(1) time, and
  updating the current thread's priority every `PRIORITY_PERIOD` (4 ticks) also
  takes O(1) time.
DISADVANTAGE:
- In `thread.c`'s `thread_set_nice`, it disables interrupts as it walks across
  `ready_list`, this walk takes O(n) time where the interrupts are disabled.
- In `thread.c`'s `calculate_load_avg` which is called once per `TIMER_FREQ`
  ticks by `thread_tick`. `calculate_load_avg` uses `list_size (&ready_list);`
  which takes O(n) time complexity to traverse through `&ready_list`.

FIXED-POINT ARITHMETIC:
ADVANTAGE:
- The arithmetic routines in `fixed-point.h` are `static inline` functions
  rather than macros, so their arguments are type-checked but there is no
  call overhead in the timer interrupt handler.
- Because `fixed_t` is a struct, a missing conversion (for example dividing a
  `fixed_t` directly by an int) is a compile error rather than a silent bug.
- `fp_mul` and `fp_div` widen to 64 bits before multiplying, so the
  intermediate cannot overflow even when the result fits in 32 bits.
- Scaling multiplies and divides by `FIXED_F` instead of shifting, because
  shifting negative values is not well defined in C and `recent_cpu` can be
  negative.
- There are separate truncating and rounding conversions to int, because the
  specification truncates when calculating priority but rounds the values
  returned by `thread_get_recent_cpu` and `thread_get_load_avg`.
DISADVANTAGE:
- There is no overflow detection or saturation. We rely on `recent_cpu` and
  `load_avg` staying far below the roughly +/-131072 range of a 17.14 value.
- The struct makes the code more verbose, since every operation has to go
  through a helper function.