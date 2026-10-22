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

Sun Young:

```c
/* Lock. */
struct lock 
  {
    struct thread *holder;      /* Thread holding lock (for debugging). */
    struct semaphore semaphore; /* Binary semaphore controlling access. */
    struct list_elem elem;      /* Element holding the lock in a holder's held 
                                   locks list.  */
    int max_waiter_priority;    /* Cached priority of the waiter with maximum priority. */
  };
```

The `lock` struct was slightly modified to enable it to be tracked by its holder in case
its holder receives donations, and to track the maximum priority of all donors waiting
for said lock.

```c
struct thread
  {
    /* Owned by thread.c. */
    tid_t tid;                 /* Thread identifier. */
    enum thread_status status; /* Thread state. */
    char name[16];             /* Name (for debugging purposes). */
    uint8_t *stack;            /* Saved stack pointer. */
    int priority;              /* Effective priority. */
    int base_priority;         /* Priority chosen by the thread. */
    int nice;                           /* Niceness of thread to other threads */
    fixed_t recent_cpu;                 /* Thread recent CPU time usage amount */
    struct list_elem allelem;  /* List element for all threads list. */
    /* Shared between thread.c and synch.c. */
    struct list_elem elem; /* List element. */

    /* Section for lock priority donation. */
    struct list held_locks;    /* List of locks currently held by this thread */
    struct lock *waiting_on;   /* The lock this thread is currently blocked on */

#ifdef USERPROG
    /* Owned by userprog/process.c. */
    struct process *process; /* Pointer to process running this thread. */
#endif

    /* Owned by thread.c. */
    unsigned magic; /* Detects stack overflow. */
};
```

The `thread` struct was modified to track a "base priority" distinct from its 
effective/donated priorities, and to track locks it is holding (to receive donations),
and the lock it is waiting on (to give donations).

Suhaan:

`int priority;` in `struct thread` (`src/threads/thread.h`)
The effective priority used by the scheduler and returned by
`thread_get_priority()`. It can be raised above the threads own priority
while a higher priority thread waits for one of its locks.

`int base_priority;` in `struct thread` (`src/threads/thread.h`)
The thread's original priority set during `thread_init()` and updated by
`thread_set_priority()`. We keep it separate so donations can temporarily
raise the effective priority without losing the value the thread chose.


---- ALGORITHMS ----

>> A2: How do you ensure that the highest priority waiting thread wakes up first
>> for a (i) semaphore, (ii) lock, or (iii) condition variable?

> **(i) semaphore**

```c
/* Compares the priorities of two threads. Returns true if thread_a has a lower
   priority than thread_b, false otherwise. */
bool
thread_elem_cmp_priority (const struct list_elem *a, const struct list_elem *b,
                          void *aux UNUSED)
{
  struct thread *thread_a = list_entry (a, struct thread, elem);
  struct thread *thread_b = list_entry (b, struct thread, elem);
  return thread_a->priority < thread_b->priority;
}
```
The `thread_elem_cmp_priority` comparator is used to find the HPW of a semaphore,
(i.e. Highest Priority Waiter, highest effective priority thread in the semaphore's waiting list.)
It does this, by getting the effective priorities of two threads in a semaphore's waiting list,
A and B, and returning whether A's effective priority is strictly less than B's.


```c
      /* Wake waiter in semaphore with highest priority */
      struct list_elem *top_priority_thread_element
          = list_max (&sema->waiters, thread_elem_cmp_priority, NULL);
      list_remove (top_priority_thread_element);
      struct thread *top_priority_thread
          = list_entry (top_priority_thread_element, struct thread, elem);
      thread_unblock (top_priority_thread);
```

The `list_max` function is used to obtain the semaphore's HPW using `thread_elem_cmp_priority`, 
then this highest priority element is "popped" or removed from the semaphore's waiter's list; 
as this runs in a critical section it will not be preempted.

> **(ii) lock**
```c
/* Releases LOCK, which must be owned by the current thread.

   An interrupt handler cannot acquire a lock, so it does not
   make sense to try to release a lock within an interrupt
   handler. */
void
lock_release (struct lock *lock)
{
  enum intr_level old_level;

  ASSERT (lock != NULL);
  ASSERT (lock_held_by_current_thread (lock));

  old_level = intr_disable ();

  lock->holder = NULL;
  list_remove (&lock->elem);
  
  /* Refresh priorities because donations to thread_current for this lock have 
     expired. */
  thread_refresh_priority (thread_current ());
  
  sema_up (&lock->semaphore); /* <--- Releases the internal semaphore */
  intr_set_level (old_level);
}
```
A lock uses a semaphore internally, which as above, already releases the semaphore's HPW.
As the semaphore's HPW is also the lock's HPW, the logic is identical.

> **(i) condition variables**

```c
/* Returns true if the thread with highest priority that waits on semaphore_elem 
   A has a lower priority than the thread with highest priority that waits on 
   semaphore_elem B. 
*/
static bool
sema_elem_cmp_priority (const struct list_elem *a, const struct list_elem *b,
                        void *aux UNUSED)
{
  struct list *waiters_a, *waiters_b;
  struct list_elem *highest_a, *highest_b;

  waiters_a = &list_entry (a, struct semaphore_elem, elem)->semaphore.waiters;
  waiters_b = &list_entry (b, struct semaphore_elem, elem)->semaphore.waiters;

  if (list_empty (waiters_a))
    return !list_empty (waiters_b);
  if (list_empty (waiters_b))
    return false;

  highest_a = list_max (waiters_a, thread_elem_cmp_priority, NULL);
  highest_b = list_max (waiters_b, thread_elem_cmp_priority, NULL);

  return thread_elem_cmp_priority (highest_a, highest_b, NULL);
}
```
The `sema_elem_cmp_priority` comparator is used to find the semaphore with the highest HPW.
It does this by comparing two semaphores A and B, and using `list_max` to find their HPWs. 
Then, it returns whether A's HPW has a strictly lower priority than B's HPW.
Ultimately, the HPW of this semaphore is also the HPW of the whole condition variable.

```c
/* If any threads are waiting on COND (protected by LOCK), then
   this function signals one of them to wake up from its wait.
   LOCK must be held before calling this function.

   An interrupt handler cannot acquire a lock, so it does not
   make sense to try to signal a condition variable within an
   interrupt handler. */
void
cond_signal (struct condition *cond, struct lock *lock UNUSED)
{
  ASSERT (cond != NULL);
  ASSERT (lock != NULL);
  ASSERT (!intr_context ());
  ASSERT (lock_held_by_current_thread (lock));

  if (!list_empty (&cond->waiters))
    {
      /* Get the waiter with the highest priority. */
      struct list_elem *highest = list_max (&cond->waiters, 
                                            sema_elem_cmp_priority, NULL);
      list_remove (highest);
      sema_up (&list_entry (highest, struct semaphore_elem, elem)->semaphore);
    }
}
```

A condition variable compares semaphores in its waiting semaphores list, to find the
semaphore with the highest priority HPW. 
Again, `list_max` with a comparator is used to perform this. T
hen, this semaphore is removed from the waiter's list, and it is upped, 
which uses the semaphore logic to unblock its HPW which is thus also the 
waiting thread with the highest priority in the whole condition variable's waiting list.

>> A3: Describe the sequence of events when a call to lock_acquire() causes a
>> priority donation. How is nested donation handled?

```c
/* Disable interrupts to ensure nested priority donation and lock acquisition
     happen atomically */
  old_level = intr_disable ();

  /* Avoids the barging problem;
  The barging problem is when the lock is finally released; but in the gap between it 
  being released the lock being acquired, another lock could acquire the lock.
  
  The purpose of the code below is: attempt to acquire the semaphore, if unsuccessful then  
  donate priorities, wait for it, and block the thread until the semaphore is successfully acquired.
  */
  while (!sema_try_down (&lock->semaphore)) 
  {
    /* sema_try_down(&lock->semaphore) returned false, implying lock->holder != NULL.
    For debugging, assert that the lock->holder != NULL. */
    ASSERT(lock->holder != NULL);
    if (!thread_mlfqs)
      {
        struct lock *curr_waiting_lock;
        struct thread *curr_waiting_lock_holder;
        
        /* Record the lock thread_current () is waiting on */
        cur->waiting_on = lock;

        /* Handles nested priority donations; donate down the chain. The
          walk stops at a lock with no holder */
        curr_waiting_lock = cur->waiting_on;
        while (curr_waiting_lock != NULL && curr_waiting_lock->holder != NULL)
          {
            curr_waiting_lock_holder = curr_waiting_lock->holder;
            if (curr_waiting_lock_holder->priority < cur->priority)
              curr_waiting_lock_holder->priority = cur->priority;

            /* Update the max waiter priority of current lock in chain,
            which may have changed due to donation */
            if (curr_waiting_lock->max_waiter_priority < cur->priority)
              curr_waiting_lock->max_waiter_priority = cur->priority;

            curr_waiting_lock = curr_waiting_lock_holder->waiting_on;
          }
      }
      list_push_back(&lock->semaphore.waiters,&cur->elem);
      thread_block();
  }
  
```

Firstly, interrupts are disabled with `intr_disable` to ensure nested priority donation and lock acquisition
happen atomically; this is because if interrupts were NOT disabled, another thread could pre-empt
the current thread and release/acquire a lock, breaking the whole chain of priority donations.

The barging problem is when the lock is finally released; but in the gap between it being released 
the lock being acquired, another lock could acquire the lock.
To avoid the barging problem, the semaphore is attempted to be acquired. If it is acquired, the code continues, 
otherwise, if the semaphore is not acquired, the donation process begins.

Then, the lock that the current thread is waiting on is recorded as `curr_waiting_lock`; through which its
holder is identified to be able to perform priority donations to said holder.

Then, a `while` loop is used to find the holder of `curr_waiting_lock`; and then, if its priority needs
donation (i.e. its priority is lower than `cur->priority`), then the current thread donates its priority to
this thread. At the same time, the maximum waiter priority of `curr_waiting_lock` is updated to reflect if
the priority of the previous waiter was changed due to donation.
Note that all donations down the priority thread come from the current waiting thread.

(While the previous waiter does not exist, it is compared to the current thread instead, but when it does,
it follows a size comparison of `previous_waiter_thread priority=curr priority` if priority WAS donated,
but if priority WAS NOT donated, `curr_priority<=previous_waiter_thread priority<=curr lock max priority`
so  the max waiting priority is NOT updated)

Finally, `curr_waiting_lock` is rereferenced to the lock that its holder is waiting on, to support nested
donations, and move down the chain. This process repeats for every lock and thread down the chain, so that
every thread receives the correct donation, and every lock stores the correct maximum waiting priority.

The thread is then blocked, to allow other threads to execute while it waits for the semaphore's release.
Once it does, it immediately tries to acquire the semaphore again, repeating until the semaphore is acquired.


>> A4: Describe the sequence of events when lock_release() is called on a lock
>> that a higher-priority thread is waiting for.

```c
/* Releases LOCK, which must be owned by the current thread.

   An interrupt handler cannot acquire a lock, so it does not
   make sense to try to release a lock within an interrupt
   handler. */
void
lock_release (struct lock *lock)
{
  enum intr_level old_level;

  ASSERT (lock != NULL);
  ASSERT (lock_held_by_current_thread (lock));

  old_level = intr_disable ();

  lock->holder = NULL;
  list_remove (&lock->elem);
  
  /* Refresh priorities because donations to thread_current for this lock have 
     expired. */
  thread_refresh_priority (thread_current ());
  
  sema_up (&lock->semaphore);
  intr_set_level (old_level);
}
```

Firstly, interrupts are disabled to prevent pre-emption (otherwise, another thread may
immediately acquire this lock).
Then, the lock's holder is set to `NULL` to reflect that the lock is not held by any thread
anymore (a lock can only be held by one thread), and the lock is removed from its holder's (which is the current thread)
"held locks" list. Now, the lock is "released". Then, as the lock was removed from the current thread,
the current thread's effective priority is refreshed to reflect that any donations made towards this thread through 
this lock are void.
Then, the semaphore of this lock is "upped" to yield to this higher priority thread.
Finally, interrupts are re-enabled.

---- SYNCHRONISATION ----

>> A5: How do you avoid a race condition in thread_set_priority() when a thread
>> needs to recompute its effective priority, but the donated priorities
>> potentially change during the computation?
>> Can you use a lock to avoid the race?

Sun Young:

A race condition is avoided in `thread_set_priority` when a thread needs to recompute its 
effective priority by disabling interrupts throughout. 

Sadly, a lock cannot be used to avoid the race. 
The race happens in `held_locks` which is where the locks the current thread holds 
(and the avenues through which it receives donations) is located.
The race surface, is sadly not in `held_locks` itself, but to `max_waiter_priority` in 
every lock in this list, which may be independently mutated by other threads when donating in 
`lock_acquire`.  
So, every lock in the `held_locks` list would need to have a lock; Having a "lock-on-lock".
Actually, now, every single one of these "locks-on-locks" would need to be acquired every single time the
thread wants to refresh priorities. This would then add to `held_locks` again, mutating it;
This violates the purpose of having the lock, as acquiring locks itself races with protecting the lock list. 
Then, the whole priority donation mechanism is reiterated through, mutating `held_locks` as we go, then the "lock-on-lock-on-lock" would be 
pushed into `held_locks` too, until infinity.

Disabling interrupts avoids this problem; as it does not mutate `held_locks`, and ensures no other threads can mutate `held_locks`.

Suhaan:

`thread_set_priority()` disables interrupts before changing `base_priority`.
It then calls `thread_refresh_priority()` which starts from that base value
and checks the effective priorities of waiters on every lock the thread
holds. The helper requires interrupts to be off, so, another thread cannot
change those waiter lists half way through the calculation. The setter also
checks `ready_list` before restoring the previous interrupt level, yielding
if a ready thread now has a higher priority.

We wouldn't use a lock for this: acquiring one could block and locks are
themselves involved in priority donation. Disabling interrupts avoids the
circular dependency and protects the calculation without introducing another
lock into the scheduler.

---- RATIONALE ----

>> A6: Why did you choose this design?
>> In what ways is it superior to another design you considered?

Sun Young: 

This design was chosen, as it was much more simple than other designs that were considered.
Initially, the use of an ordered list was considered for the waiter's lists and ready lists, however, this soon 
proved to be difficult to handle upon priority donation; mutating the priorities of the ordered lists themselves would 
break the order, requiring O(n) re-insertion into said lists. 
Simple lists would require minimal modifications to the lock, semaphore, and thread structs, to enhance compatibility with 
existing code, and make current code easier to understand. 
And in addition, we considered overlooking implementation of MPW caching. However, this would transform a single-loop into a 
two-layer nested loop, which would prove to be rather inefficient when executed thousands of times by the scheduler, and when 
a large number of threads are queueing to access a certain lock.

Suhaan:

We kept `priority` as the effective value read by the scheduler and getter,
with `base_priority` remembering the threads own setting. When that setting
changes, we recalculate the effective value from threads currently
waiting on locks the thread holds. Unlike a cached maximum donation, this
also takes account of donations disappearing when a lock is released without
having to maintain a separate list of donation records.

The setter checks the entire ready list instead of just its front: the list
is not kept sorted (`thread_unblock()` appends to it) and the scheduler also
searches it for the highest priority. This is simpler to keep consistent,
but the cost is a linear scan of ready threads on top of examining held locks
and their waiters, while interrupts are disabled. For our getter and setter work
we preferred that tradeoff to extra cached donation state or keeping every
queue sorted after priority changes.

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

`static struct thread *charged_threads[PRIORITY_PERIOD]` (thread.c)
   To store the threads charged a timer tick since the last fourth tick. Only
   these threads can have a priority which no longer matches the BSD formula,
   for which these threads (at most PRIORITY_PERIOD (4) entries) are 
   recalculated every PRIORITY_PERIOD's ticks in `thread_tick`.

`static int charged_cnt;` (thread.c)
   To represent the number of entries in `charged_threads`. Resets to 0 after
   every PRIORITY_PERIOD's recalculation in `thread_tick (void)`.

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

On the contrary, in the context of every `PRIORITY_PERIOD` (4) ticks, we only 
recalculate the priority of the threads in `charged_threads` in 
`thread_tick ()`. Because `charged_threads` only holds every thread whose 
`priority` no longer match the BSD formula since the last `PRIORITY_PERIOD` (4) 
ticks. We minimise the amount of time spent in timer interrupt handler, by
ensuring that we only recalculate the priority for the least amount of threads 
needed every 4 ticks (when `timer_ticks () % PRIORITY_PERIOD == 0`). 

run a targeted `update_thread_priority (t, NULL);` which only recalculates priority for the
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
  updating the priorities of the at most 4 (`PRIORITY_PERIOD`) threads in 
  `charged_threads` every `PRIORITY_PERIOD` (4 ticks) also only takes O(1) time.
  
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