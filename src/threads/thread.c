#include "threads/thread.h"
#include "devices/timer.h"
#include "threads/fixed-point.h"
#include "threads/flags.h"
#include "threads/interrupt.h"
#include "threads/intr-stubs.h"
#include "threads/palloc.h"
#include "threads/switch.h"
#include "threads/synch.h"
#include "threads/vaddr.h"
#include <debug.h>
#include <random.h>
#include <stddef.h>
#include <stdio.h>
#include <string.h>

/* Random value for struct thread's `magic' member.
   Used to detect stack overflow.  See the big comment at the top
   of thread.h for details. */
#define THREAD_MAGIC 0xcd6abf4b

/* Time period at which thread priorities are updated. */
#define PRIORITY_PERIOD 4

/* Returns the maximum or minimum of A and B, respectively */
#define MAX(A, B) ((A) > (B) ? (A) : (B))
#define MIN(A, B) ((A) > (B) ? (B) : (A))

/* List of processes in THREAD_READY state, that is, processes
   that are ready to run but not actually running. */
static struct list ready_list;

/* List of all processes.  Processes are added to this list
   when they are first scheduled and removed when they exit. */
static struct list all_list;

/* Idle thread. */
static struct thread *idle_thread;

/* Initial thread, the thread running init.c:main(). */
static struct thread *initial_thread;

/* Lock used by allocate_tid(). */
static struct lock tid_lock;

/* System load average, estimates the average number of threads ready to run
   the past minute */
static fixed_t load_avg;


/* Threads charged a timer tick since the last fourth tick.  Only these can
   have a priority that no longer matches the BSD formula, and there are at
   most PRIORITY_PERIOD of them. */
static struct thread *charged_threads[PRIORITY_PERIOD];
static int charged_cnt;

/* Stack frame for kernel_thread(). */
struct kernel_thread_frame
{
  void *eip;             /* Return address. */
  thread_func *function; /* Function to call. */
  void *aux;             /* Auxiliary data for function. */
};

/* Statistics. */
static long long idle_ticks;   /* # of timer ticks spent idle. */
static long long kernel_ticks; /* # of timer ticks in kernel threads. */
static long long user_ticks;   /* # of timer ticks in user programs. */

/* Scheduling. */
#define TIME_SLICE 4          /* # of timer ticks to give each thread. */
static unsigned thread_ticks; /* # of timer ticks since last yield. */

/* If false (default), use round-robin scheduler.
   If true, use multi-level feedback queue scheduler.
   Controlled by kernel command-line option "-mlfqs". */
bool thread_mlfqs;

static void kernel_thread (thread_func *, void *aux);

static void idle (void *aux UNUSED);
static struct thread *running_thread (void);
static struct thread *next_thread_to_run (void);
static void thread_init (struct thread *, const char *name, int priority);
static bool is_thread (struct thread *);
static void *alloc_frame (struct thread *, size_t size);
static void schedule (void);
void thread_schedule_tail (struct thread *prev);
static tid_t allocate_tid (void);
static void calculate_recent_cpu_and_priorities (void);
static void calculate_load_avg (void);
static void update_thread_recent_cpu_and_priority (struct thread *t,
                                                   void *aux);
static void update_thread_priority (struct thread *t, void *aux UNUSED);
#ifdef USERPROG
static bool is_user_mode (struct thread *);
#endif

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

/* Initializes the threading system by transforming the code
   that's currently running into a thread.  This can't work in
   general and it is possible in this case only because loader.S
   was careful to put the bottom of the stack at a page boundary.

   Also initializes the run queue and the tid lock.

   After calling this function, be sure to initialize the page
   allocator before trying to create any threads with
   thread_create().

   It is not safe to call thread_current() until this function
   finishes. */
void
threading_init (void)
{
  ASSERT (intr_get_level () == INTR_OFF);

  lock_init (&tid_lock);
  list_init (&ready_list);
  list_init (&all_list);

  /* Set up a thread structure for the running thread. */
  initial_thread = running_thread ();
  thread_init (initial_thread, "main", PRI_DEFAULT);
  initial_thread->status = THREAD_RUNNING;
  initial_thread->tid = allocate_tid ();
}

/* Starts preemptive thread scheduling by enabling interrupts.
   Also creates the idle thread. */
void
threading_start (void)
{
  /* Create the idle thread. */
  struct semaphore idle_started;
  sema_init (&idle_started, 0);
  thread_create ("idle", PRI_MIN, idle, &idle_started);

  /* Start preemptive thread scheduling. */
  intr_enable ();

  /* Wait for the idle thread to initialize idle_thread. */
  sema_down (&idle_started);
}

/* Returns the number of threads currently in the ready list.
   Disables interrupts to avoid any race-conditions on the ready list. */
size_t
threads_ready (void)
{
  enum intr_level old_level = intr_disable ();
  size_t ready_thread_count = list_size (&ready_list);
  intr_set_level (old_level);
  return ready_thread_count;
}

/* Called by the timer interrupt handler at each timer tick.
   Thus, this function runs in an external interrupt context. */
void
thread_tick (void)
{
  struct thread *t = thread_current ();

  /* Update statistics. */
  if (t == idle_thread)
    idle_ticks++;
#ifdef USERPROG
  else if (is_user_mode (t))
    user_ticks++;
#endif
  else
    kernel_ticks++;

  if (thread_mlfqs)
    {
      if (t != idle_thread)
        {
          t->recent_cpu = fp_add_int (t->recent_cpu, 1);
          if (charged_cnt == 0 || charged_threads[charged_cnt - 1] != t)
            {
              ASSERT (charged_cnt < PRIORITY_PERIOD);
              charged_threads[charged_cnt++] = t;
            }
        }

      /* Every TIMER_FREQ ticks, recalculates load_avg, then every thread's
         recent_cpu and priority.  Every other fourth tick, recalculates the
         priority of each thread charged a tick since the last one. */
      if (timer_ticks () % TIMER_FREQ == 0)
        {
          calculate_load_avg ();
          calculate_recent_cpu_and_priorities ();
          charged_cnt = 0;
        }
      else if (timer_ticks () % PRIORITY_PERIOD == 0)
        {
          /* Only threads charged a tick since the last fourth tick can have
             a stale priority, so recalculate just those. */
          for (int i = 0; i < charged_cnt; i++)
            if (charged_threads[i] != NULL)
              update_thread_priority (charged_threads[i], NULL);
          charged_cnt = 0;
        }
    }

  /* Enforce preemption. */
  if (++thread_ticks >= TIME_SLICE)
    intr_yield_on_return ();
}

/* Prints thread statistics. */
void
thread_print_stats (void)
{
  printf ("Thread: %lld idle ticks, %lld kernel ticks, %lld user ticks\n",
          idle_ticks, kernel_ticks, user_ticks);
}

/* Creates a new kernel thread named NAME with the given initial
   PRIORITY, which executes FUNCTION passing AUX as the argument,
   and adds it to the ready queue.  Returns the thread identifier
   for the new thread, or TID_ERROR if creation fails.

   If threading_start() has been called, then the new thread may be
   scheduled before thread_create() returns.  It could even exit
   before thread_create() returns.  Contrariwise, the original
   thread may run for any amount of time before the new thread is
   scheduled.  Use a semaphore or some other form of
   synchronization if you need to ensure ordering.

   The code provided sets the new thread's `priority' member to
   PRIORITY, but no actual priority scheduling is implemented.
   Priority scheduling is the goal of Problem 1-3. */
tid_t
thread_create (const char *name, int priority, thread_func *function,
               void *aux)
{
  struct thread *t;
  struct kernel_thread_frame *kf;
  struct switch_entry_frame *ef;
  struct switch_threads_frame *sf;
  tid_t tid;
  enum intr_level old_level;

  ASSERT (function != NULL);

  /* Allocate thread. */
  t = palloc_get_page (PAL_ZERO);
  if (t == NULL)
    return TID_ERROR;

  /* Initialize thread. */
  thread_init (t, name, priority);
  tid = t->tid = allocate_tid ();

  /* Prepare thread for first run by initializing its stack.
     Do this atomically so intermediate values for the 'stack'
     member cannot be observed. */
  old_level = intr_disable ();

  /* Stack frame for kernel_thread(). */
  kf = alloc_frame (t, sizeof *kf);
  kf->eip = NULL;
  kf->function = function;
  kf->aux = aux;

  /* Stack frame for switch_entry(). */
  ef = alloc_frame (t, sizeof *ef);
  ef->eip = (void (*) (void))kernel_thread;

  /* Stack frame for switch_threads(). */
  sf = alloc_frame (t, sizeof *sf);
  sf->eip = switch_entry;
  sf->ebp = 0;

  intr_set_level (old_level);

  /* Add to run queue. */
  thread_unblock (t);

  /* Yield to the new thread if it has higher priority. */
  if (thread_get_priority () < t->priority)
    thread_yield ();
  return tid;
}

/* Puts the current thread to sleep.  It will not be scheduled
   again until awoken by thread_unblock().

   This function must be called with interrupts turned off.  It
   is usually a better idea to use one of the synchronization
   primitives in synch.h. */
void
thread_block (void)
{
  ASSERT (!intr_context ());
  ASSERT (intr_get_level () == INTR_OFF);

  thread_current ()->status = THREAD_BLOCKED;
  schedule ();
}

/* Transitions a blocked thread T to the ready-to-run state.
   This is an error if T is not blocked.  (Use thread_yield() to
   make the running thread ready.)

   This function does not preempt the running thread.  This can
   be important: if the caller had disabled interrupts itself,
   it may expect that it can atomically unblock a thread and
   update other data. */
void
thread_unblock (struct thread *t)
{
  enum intr_level old_level;

  ASSERT (is_thread (t));

  old_level = intr_disable ();
  ASSERT (t->status == THREAD_BLOCKED);
  list_push_back (&ready_list, &t->elem);
  t->status = THREAD_READY;
  intr_set_level (old_level);
}

/* Returns the name of the running thread. */
const char *
thread_name (void)
{
  return thread_current ()->name;
}

/* Returns the running thread.
   This is running_thread() plus a couple of sanity checks.
   See the big comment at the top of thread.h for details. */
struct thread *
thread_current (void)
{
  struct thread *t = running_thread ();

  /* Make sure T is really a thread.
     If either of these assertions fire, then your thread may
     have overflowed its stack.  Each thread has less than 4 kB
     of stack, so a few big automatic arrays or moderate
     recursion can cause stack overflow. */
  ASSERT (is_thread (t));
  ASSERT (t->status == THREAD_RUNNING);

  return t;
}

/* Returns the running thread's tid. */
tid_t
thread_tid (void)
{
  return thread_current ()->tid;
}

/* Deschedules the current thread and destroys it.  Never
   returns to the caller. */
void
thread_exit (void)
{
  ASSERT (!intr_context ());

  /* Remove thread from all threads list, set our status to dying,
     and schedule another process.  That process will destroy us
     when it calls thread_schedule_tail(). */
  intr_disable ();

  /* This thread's page is freed once we switch away, so it must not be
     left in charged_threads for the next fourth tick to update. */
  for (int i = 0; i < charged_cnt; i++)
    if (charged_threads[i] == thread_current ())
      charged_threads[i] = NULL;

  list_remove (&thread_current ()->allelem);
  thread_current ()->status = THREAD_DYING;
  schedule ();
  NOT_REACHED ();
}

/* Yields the CPU.  The current thread is not put to sleep and
   may be scheduled again immediately at the scheduler's whim. */
void
thread_yield (void)
{
  struct thread *cur = thread_current ();
  enum intr_level old_level;

  ASSERT (!intr_context ());

  old_level = intr_disable ();
  if (cur != idle_thread)
    list_push_back (&ready_list, &cur->elem);
  cur->status = THREAD_READY;
  schedule ();
  intr_set_level (old_level);
}

/* Invoke function 'func' on all threads, passing along 'aux'.
   This function must be called with interrupts off. */
void
thread_foreach (thread_action_func *func, void *aux)
{
  struct list_elem *e;

  ASSERT (intr_get_level () == INTR_OFF);

  for (e = list_begin (&all_list); e != list_end (&all_list);
       e = list_next (e))
    {
      struct thread *t = list_entry (e, struct thread, allelem);
      func (t, aux);
    }
}

/* Recalculates the effective priority of thread T after a priority donation
   is added or removed */
void
thread_refresh_priority (struct thread *t)
{
  struct list_elem *lock_elem;

  ASSERT (t != NULL);
  ASSERT (intr_get_level () == INTR_OFF);

  /* The 4.4BSD scheduler sets priorities itself, and doesn't donate */
  if (thread_mlfqs)
    return;

  ASSERT (t->base_priority >= PRI_MIN && t->base_priority <= PRI_MAX);

  t->priority = t->base_priority;
  for (lock_elem = list_begin (&t->held_locks);
       lock_elem != list_end (&t->held_locks);
       lock_elem = list_next (lock_elem))
    {
      struct lock *current_lock = list_entry (lock_elem, struct lock, elem);

      if (t->priority < current_lock->max_waiter_priority)
        t->priority = current_lock->max_waiter_priority;
    }
}

/* Changes the current thread's base priority without losing donations */
void
thread_set_priority (int new_priority)
{
  enum intr_level old_level;
  struct list_elem *e;
  bool should_yield = false;
  struct thread *cur = thread_current ();

  ASSERT (!intr_context ());

  if (thread_mlfqs || new_priority < PRI_MIN || new_priority > PRI_MAX)
    return;

  old_level = intr_disable ();
  cur->base_priority = new_priority;
  thread_refresh_priority (cur);

  for (e = list_begin (&ready_list); e != list_end (&ready_list);
       e = list_next (e))
    {
      struct thread *ready = list_entry (e, struct thread, elem);
      if (ready->priority > cur->priority)
        {
          should_yield = true;
          break;
        }
    }
  if (should_yield)
    thread_yield ();

  intr_set_level (old_level);
}

/* Returns the current thread's priority. */
int
thread_get_priority (void)
{
  return thread_current ()->priority;
}

/* Sets the current thread's nice value to NICE. */
void
thread_set_nice (int nice)
{
  enum intr_level old_level;
  struct thread *cur = thread_current ();
  bool yield_needed = false;

  old_level = intr_disable ();
  cur->nice = nice;
  if (thread_mlfqs)
    {
      update_thread_priority (cur, NULL);
    }

  /* If running thread no longer has the highest priority, yields */
  for (struct list_elem *elem = list_begin (&ready_list);
       elem != list_end (&ready_list); elem = list_next (elem))
    {
      struct thread *elem_thread = list_entry (elem, struct thread, elem);
      if (elem_thread->priority > cur->priority)
        {
          yield_needed = true;
          break;
        }
    }

  intr_set_level (old_level);

  if (yield_needed)
    thread_yield ();
}

/* Returns the current thread's nice value. */
int
thread_get_nice (void)
{
  struct thread *cur = thread_current ();
  return cur->nice;
}

/* Returns 100 times the system load average. */
int
thread_get_load_avg (void)
{
  return fp_to_int_round (fp_mul_int (load_avg, 100));
}

/* Returns 100 times the current thread's recent_cpu value. */
int
thread_get_recent_cpu (void)
{
  struct thread *cur = thread_current ();
  return fp_to_int_round (fp_mul_int (cur->recent_cpu, 100));
}

/* Idle thread.  Executes when no other thread is ready to run.

   The idle thread is initially put on the ready list by
   threading_start().  It will be scheduled once initially, at which
   point it initializes idle_thread, "up"s the semaphore passed
   to it to enable threading_start() to continue, and immediately
   blocks.  After that, the idle thread never appears in the
   ready list.  It is returned by next_thread_to_run() as a
   special case when the ready list is empty. */
static void
idle (void *idle_started_ UNUSED)
{
  struct semaphore *idle_started = idle_started_;
  idle_thread = thread_current ();
  sema_up (idle_started);

  for (;;)
    {
      /* Let someone else run. */
      intr_disable ();
      thread_block ();

      /* Re-enable interrupts and wait for the next one.

         The `sti' instruction disables interrupts until the
         completion of the next instruction, so these two
         instructions are executed atomically.  This atomicity is
         important; otherwise, an interrupt could be handled
         between re-enabling interrupts and waiting for the next
         one to occur, wasting as much as one clock tick worth of
         time.

         See [IA32-v2a] "HLT", [IA32-v2b] "STI", and [IA32-v3a]
         7.11.1 "HLT Instruction". */
      asm volatile ("sti; hlt" : : : "memory");
    }
}

/* Function used as the basis for a kernel thread. */
static void
kernel_thread (thread_func *function, void *aux)
{
  ASSERT (function != NULL);

  intr_enable (); /* The scheduler runs with interrupts off. */
  function (aux); /* Execute the thread function. */
  thread_exit (); /* If function() returns, kill the thread. */
}

/* Returns the running thread. */
struct thread *
running_thread (void)
{
  uint32_t *esp;

  /* Copy the CPU's stack pointer into `esp', and then round that
     down to the start of a page.  Because `struct thread' is
     always at the beginning of a page and the stack pointer is
     somewhere in the middle, this locates the curent thread. */
  asm ("mov %%esp, %0" : "=g"(esp));
  return pg_round_down (esp);
}

/* Returns true if T appears to point to a valid thread. */
static bool
is_thread (struct thread *t)
{
  return t != NULL && t->magic == THREAD_MAGIC;
}

#ifdef USERPROG
/* Returns true if T appears to be running a valid user process. */
static bool
is_user_mode (struct thread *t)
{
  return t->process && t->process->pagedir;
}
#endif

/* Does basic initialisation of T as a blocked thread named NAME. */
static void
thread_init (struct thread *t, const char *name, int priority)
{
  enum intr_level old_level;

  ASSERT (t != NULL);
  ASSERT (PRI_MIN <= priority && priority <= PRI_MAX);
  ASSERT (name != NULL);

  memset (t, 0, sizeof *t);
  t->status = THREAD_BLOCKED;
  strlcpy (t->name, name, sizeof t->name);
  t->stack = (uint8_t *)t + PGSIZE;
  t->priority = priority;
  t->base_priority = priority;
  t->nice = 0;
  t->recent_cpu = fp_from_int (0);
  if (thread_mlfqs)
    {
      if (t != initial_thread)
        {
          struct thread *parent = thread_current ();
          t->nice = parent->nice;
          t->recent_cpu = parent->recent_cpu;
        }

      /* In BSD mode, ignore PRIORITY function param and calculate priority
          instead */
      update_thread_priority (t, NULL);
    }
  list_init (&t->held_locks);
  t->waiting_on = NULL;
#ifdef USERPROG
  t->process = NULL;
#endif
  list_elem_init (&t->allelem);
  list_elem_init (&t->elem);
  t->magic = THREAD_MAGIC;

  old_level = intr_disable ();
  list_push_back (&all_list, &t->allelem);
  intr_set_level (old_level);
}

/* Allocates a SIZE-byte frame at the top of thread T's stack and
   returns a pointer to the frame's base. */
static void *
alloc_frame (struct thread *t, size_t size)
{
  /* Stack data is always allocated in word-size units. */
  ASSERT (is_thread (t));
  ASSERT (size % sizeof (uint32_t) == 0);

  t->stack -= size;
  return t->stack;
}

/* Chooses and returns the next thread to be scheduled.  Should
   return a thread from the run queue, unless the run queue is
   empty.  (If the running thread can continue running, then it
   will be in the run queue.)  If the run queue is empty, return
   idle_thread. */
static struct thread *
next_thread_to_run (void)
{
  struct list_elem *highest;

  if (list_empty (&ready_list))
    return idle_thread;

  highest = list_max (&ready_list, thread_elem_cmp_priority, NULL);
  list_remove (highest);
  return list_entry (highest, struct thread, elem);
}

/* Completes a thread switch by activating the new thread's page
   tables, and, if the previous thread is dying, destroying it.

   At this function's invocation, we just switched from thread
   PREV, the new thread is already running, and interrupts are
   still disabled.  This function is normally invoked by
   thread_schedule() as its final action before returning, but
   the first time a thread is scheduled it is called by
   switch_entry() (see switch.S).

   It's not safe to call printf() until the thread switch is
   complete.  In practice that means that printf()s should be
   added at the end of the function.

   After this function and its caller returns, the thread switch
   is complete. */
void
thread_schedule_tail (struct thread *prev)
{
  struct thread *cur = running_thread ();

  ASSERT (intr_get_level () == INTR_OFF);

  /* Mark us as running. */
  cur->status = THREAD_RUNNING;

  /* Start new time slice. */
  thread_ticks = 0;

#ifdef USERPROG
  /* Activate the new address space. */
  process_activate ();
#endif

  /* If the thread we switched from is dying, destroy its struct
     thread.  This must happen late so that thread_exit() doesn't
     pull out the rug under itself.  (We don't free
     initial_thread because its memory was not obtained via
     palloc().) */
  if (prev != NULL && prev->status == THREAD_DYING && prev != initial_thread)
    {
      ASSERT (prev != cur);
      palloc_free_page (prev);
    }
}

/* Schedules a new process.  At entry, interrupts must be off and
   the running process's state must have been changed from
   running to some other state.  This function finds another
   thread to run and switches to it.

   It's not safe to call printf() until thread_schedule_tail()
   has completed. */
static void
schedule (void)
{
  struct thread *cur = running_thread ();
  struct thread *next = next_thread_to_run ();
  struct thread *prev = NULL;

  ASSERT (intr_get_level () == INTR_OFF);
  ASSERT (cur->status != THREAD_RUNNING);
  ASSERT (is_thread (next));

  if (cur != next)
    prev = switch_threads (cur, next);
  thread_schedule_tail (prev);
}

/* Returns a tid to use for a new thread. */
static tid_t
allocate_tid (void)
{
  static tid_t next_tid = 1;
  tid_t tid;

  lock_acquire (&tid_lock);
  tid = next_tid++;
  lock_release (&tid_lock);

  return tid;
}

/* Recalculates every thread's recent_cpu and then its priority, once per
   TIMER_FREQ timer ticks, using a single walk of all_list.  The load_avg
   coefficient is computed once here rather than once per thread. */
static void
calculate_recent_cpu_and_priorities (void)
{
  fixed_t load_avg_coeff = fp_div (fp_mul_int (load_avg, 2),
                                   fp_add_int (fp_mul_int (load_avg, 2), 1));
  thread_foreach (&update_thread_recent_cpu_and_priority, &load_avg_coeff);
}

/* Calculate latest `load_avg` once per TIMER_FREQ timer ticks. */
static void
calculate_load_avg (void)
{
  struct thread *cur = thread_current ();
  fixed_t load_avg_frac = fp_div_int (fp_from_int (59), 60);
  int ready_threads;

  ready_threads = list_size (&ready_list);
  if (cur != idle_thread)
    ready_threads++;

  /* load_avg = (59/60)*load_avg + (1/60)*ready_threads */
  load_avg = fp_add (fp_mul (load_avg_frac, load_avg),
                     fp_div_int (fp_from_int (ready_threads), 60));
}

/* Pass in as function pointer of type `thread_action_func` to calculate
   each thread's recent_cpu, then its priority from the new value.  AUX
   points to the load_avg coefficient. */
static void
update_thread_recent_cpu_and_priority (struct thread *t, void *aux)
{
  fixed_t load_avg_coeff = *(fixed_t *) aux;

  if (t == idle_thread)
    return;

  /* recent_cpu = (2*load_avg) / (2*load_avg + 1) * recent_cpu + nice */
  t->recent_cpu = fp_add_int (fp_mul (load_avg_coeff, t->recent_cpu), t->nice);

  update_thread_priority (t, NULL);
}

/* Recalculates T's priority from its recent_cpu and nice value, clamped to
   the range PRI_MIN to PRI_MAX.  AUX is unused. */
static void
update_thread_priority (struct thread *t, void *aux UNUSED)
{
  if (t == idle_thread)
    return;

  /* priority = PRI_MAX - (recent_cpu / 4) - (nice * 2) */
  fixed_t penalty = fp_add_int (fp_div_int (t->recent_cpu, 4), t->nice * 2);
  t->priority = fp_to_int_trunc (fp_sub (fp_from_int (PRI_MAX), penalty));
  t->priority = MAX (PRI_MIN, MIN (t->priority, PRI_MAX));
}

/* Offset of `stack' member within `struct thread'.
   Used by switch.S, which can't figure it out on its own. */
uint32_t thread_stack_ofs = offsetof (struct thread, stack);
