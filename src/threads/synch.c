/* This file is derived from source code for the Nachos
   instructional operating system.  The Nachos copyright notice
   is reproduced in full below. */

/* Copyright (c) 1992-1996 The Regents of the University of California.
   All rights reserved.

   Permission to use, copy, modify, and distribute this software
   and its documentation for any purpose, without fee, and
   without written agreement is hereby granted, provided that the
   above copyright notice and the following two paragraphs appear
   in all copies of this software.

   IN NO EVENT SHALL THE UNIVERSITY OF CALIFORNIA BE LIABLE TO
   ANY PARTY FOR DIRECT, INDIRECT, SPECIAL, INCIDENTAL, OR
   CONSEQUENTIAL DAMAGES ARISING OUT OF THE USE OF THIS SOFTWARE
   AND ITS DOCUMENTATION, EVEN IF THE UNIVERSITY OF CALIFORNIA
   HAS BEEN ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

   THE UNIVERSITY OF CALIFORNIA SPECIFICALLY DISCLAIMS ANY
   WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
   WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
   PURPOSE.  THE SOFTWARE PROVIDED HEREUNDER IS ON AN "AS IS"
   BASIS, AND THE UNIVERSITY OF CALIFORNIA HAS NO OBLIGATION TO
   PROVIDE MAINTENANCE, SUPPORT, UPDATES, ENHANCEMENTS, OR
   MODIFICATIONS.
*/

#include "threads/synch.h"
#include <stdio.h>
#include <string.h>
#include "threads/interrupt.h"
#include "threads/thread.h"

/* Initializes semaphore SEMA to VALUE.  A semaphore is a
   nonnegative integer along with two atomic operators for
   manipulating it:

   - down or "P": wait for the value to become positive, then
     decrement it.

   - up or "V": increment the value (and wake up one waiting
     thread, if any). */
void
sema_init (struct semaphore *sema, unsigned value) 
{
  ASSERT (sema != NULL);

  sema->value = value;
  list_init (&sema->waiters);
}

/* Down or "P" operation on a semaphore.  Waits for SEMA's value
   to become positive and then atomically decrements it.

   This function may sleep, so it must not be called within an
   interrupt handler.  This function may be called with
   interrupts disabled, but if it sleeps then the next scheduled
   thread will probably turn interrupts back on. */
void
sema_down (struct semaphore *sema) 
{
  enum intr_level old_level;

  ASSERT (sema != NULL);
  ASSERT (!intr_context ());

  old_level = intr_disable ();
  while (sema->value == 0) 
    {
      /* Insert the thread, with the largest priority, into the queue first;
      Ensuring that the queue is sorted from biggest priority, down to smallest. */
      
      struct thread* thread_curr = thread_current();

      printf("I'm pushing current thread %s with priority %d into the wait queue\n", &thread_curr->name, thread_curr->priority);

      list_push_back(&sema->waiters, &thread_current()->elem);
      // list_insert_ordered(&sema->waiters, &thread_current ()->elem, thread_elem_cmp_priority, &cmp_greater_than);
      thread_block ();
    }
  sema->value--;
  intr_set_level (old_level);
}

/* Down or "P" operation on a semaphore, but only if the
   semaphore is not already 0.  Returns true if the semaphore is
   decremented, false otherwise.

   This function may be called from an interrupt handler. */
bool
sema_try_down (struct semaphore *sema) 
{
  enum intr_level old_level;
  bool success;

  ASSERT (sema != NULL);

  old_level = intr_disable ();
  if (sema->value > 0) 
    {
      sema->value--;
      success = true; 
    }
  else
    success = false;
  intr_set_level (old_level);

  return success;
}

/* Up or "V" operation on a semaphore.  Increments SEMA's value
   and wakes up one thread of those waiting for SEMA, if any.

   This function may be called from an interrupt handler. */
void
sema_up (struct semaphore *sema) 
{
  enum intr_level old_level;

  ASSERT (sema != NULL);

  old_level = intr_disable ();
  
  
  bool should_yield = false;
  if (!list_empty(&sema->waiters))  { 
        

        /* Do some magic, to get the top priority element of the thread! */

       struct list_elem* top_priority_thread_element = list_max(&sema->waiters, thread_elem_cmp_priority, &cmp_less_than);

        list_remove(top_priority_thread_element); /* Remove this element. */

        struct thread* top_priority_thread = list_entry (top_priority_thread_element,
                                struct thread, elem); 
        
        /* Unblock said top priority thread! */
        printf("I'm trying to unblock thread %s \n", top_priority_thread->name);

        thread_unblock (top_priority_thread); 
        
        /* Should we yield? */
        should_yield = thread_get_priority() <= top_priority_thread->priority;
  }
  sema->value++;
  intr_set_level (old_level);

  /* We've just upped our semaphore; 
  Yield our execution if this thread's priority is higher than ours. */
  if (should_yield) thread_yield();
}

static void sema_test_helper (void *sema_);

/* Self-test for semaphores that makes control "ping-pong"
   between a pair of threads.  Insert calls to printf() to see
   what's going on. */
void
sema_self_test (void) 
{
  struct semaphore sema[2];
  int i;

  printf ("Testing semaphores...");
  sema_init (&sema[0], 0);
  sema_init (&sema[1], 0);
  thread_create ("sema-test", PRI_DEFAULT, sema_test_helper, &sema);
  for (i = 0; i < 10; i++) 
    {
      sema_up (&sema[0]);
      sema_down (&sema[1]);
    }
  printf ("done.\n");
}

/* Thread function used by sema_self_test(). */
static void
sema_test_helper (void *sema_) 
{
  struct semaphore *sema = sema_;
  int i;

  for (i = 0; i < 10; i++) 
    {
      sema_down (&sema[0]);
      sema_up (&sema[1]);
    }
}

/* Initializes LOCK.  A lock can be held by at most a single
   thread at any given time.  Our locks are not "recursive", that
   is, it is an error for the thread currently holding a lock to
   try to acquire that lock.

   A lock is a specialization of a semaphore with an initial
   value of 1.  The difference between a lock and such a
   semaphore is twofold.  First, a semaphore can have a value
   greater than 1, but a lock can only be owned by a single
   thread at a time.  Second, a semaphore does not have an owner,
   meaning that one thread can "down" the semaphore and then
   another one "up" it, but with a lock the same thread must both
   acquire and release it.  When these restrictions prove
   onerous, it's a good sign that a semaphore should be used,
   instead of a lock. */
void
lock_init (struct lock *lock)
{
  ASSERT (lock != NULL);

  lock->holder = NULL;
  sema_init (&lock->semaphore, 1);
}

/* Acquires LOCK, sleeping until it becomes available if
   necessary.  The lock must not already be held by the current
   thread.

   This function may sleep, so it must not be called within an
   interrupt handler.  This function may be called with
   interrupts disabled, but interrupts will be turned back on if
   we need to sleep. */
void
lock_acquire (struct lock *lock)
{
  ASSERT (lock != NULL);
  ASSERT (!intr_context ());
  ASSERT (!lock_held_by_current_thread (lock));

  struct thread* curr_thread = thread_current ();

  /* Critical section, disable interrupts! 
  We need this to be able to donate the priorities without pre-emption.
  */
  enum intr_level old_level = intr_disable();

 /* Donate our priorities BEFORE we down the semaphore. */
 if (!thread_mlfqs && lock->holder != NULL) {
    /* Then, we're waiting on the lock's holder. */
    curr_thread->waiting_on = lock;
    /* This section handles nested priority donations; donate down the chain. */
    struct lock* curr_waiting_lock = curr_thread->waiting_on;
    while (curr_waiting_lock != NULL) {
      /* Who's holding the lock I'm waiting on?*/

      struct thread* curr_waiting_lock_holder = curr_waiting_lock->holder;

      /* Now, update the priority of the current donation thread.*/
      
      if (curr_waiting_lock_holder->priority < curr_thread->priority) curr_waiting_lock_holder->priority = curr_thread->priority;

      /* Update complete. */

      curr_waiting_lock = curr_waiting_lock_holder->waiting_on;
      /* Now, who's the next guy up the chain? */
    }
 }

 intr_set_level(old_level);
/* End of the critical section, re-enable interrupts. */
  sema_down (&lock->semaphore);
  curr_thread->waiting_on = NULL; // I'm not waiting on any lock because I've already got it.
  lock->holder = curr_thread;
  list_push_back(&curr_thread->held_locks, &lock->elem);
  /* Store the list of held locks, so I know that I'm holding it. */
}

/* Tries to acquires LOCK and returns true if successful or false
   on failure.  The lock must not already be held by the current
   thread.

   This function will not sleep, so it may be called within an
   interrupt handler. */
bool
lock_try_acquire (struct lock *lock)
{
  bool success;

  ASSERT (lock != NULL);
  ASSERT (!lock_held_by_current_thread (lock));

  success = sema_try_down (&lock->semaphore);
  if (success) {
    struct thread* curr_thread = thread_current ();
    lock->holder = curr_thread;
    list_push_back(&curr_thread->held_locks, &lock->elem);
    /* Store the list of held locks, so I know that I'm holding it. */
  }
  return success;
}

/* Releases LOCK, which must be owned by the current thread.

   An interrupt handler cannot acquire a lock, so it does not
   make sense to try to release a lock within an interrupt
   handler. */
void
lock_release (struct lock *lock) 
{
  ASSERT (lock != NULL);
  ASSERT (lock_held_by_current_thread (lock));

  /* Critical Section! Remove this element from the list.*/
  enum intr_level old_level = intr_disable();

  /* Remove the current lock from the holding thread's held locks list. */
  list_remove(&lock->elem);

  /* Refresh my priorities, because my donation is expired. */
  thread_refresh_priority(thread_current());

  lock->holder = NULL;
  intr_set_level(old_level);
  /* End of the critical section. */
  sema_up (&lock->semaphore);
}

/* Returns true if the current thread holds LOCK, false
   otherwise.  (Note that testing whether some other thread holds
   a lock would be racy.) */
bool
lock_held_by_current_thread (const struct lock *lock) 
{
  ASSERT (lock != NULL);

  return lock->holder == thread_current ();
}

/* One semaphore in a list. */
struct semaphore_elem 
  {
    struct list_elem elem;              /* List element. */
    struct semaphore semaphore;         /* This semaphore. */
  };

/* Initializes condition variable COND.  A condition variable
   allows one piece of code to signal a condition and cooperating
   code to receive the signal and act upon it. */
void
cond_init (struct condition *cond)
{
  ASSERT (cond != NULL);

  list_init (&cond->waiters);
}


/*
Compares the priorities of two waiting semaphore elements by the priority of the thread in their semaphore's waiting list with the largest priority.
'aux' here signifies whether to compare as "less", or "more".
(i.e., whether to return true on when priority of 'A' is less or larger than priority of 'B'.)

When 'aux' is set to true, it returns whether 'A' priority is less than 'B' priority.
Otherwise, if 'aux' is false, it returns whether 'A' priority is gte. 'B' priority.
*/
bool 
sema_elem_cmp_priority (const struct list_elem *a,       const struct list_elem *b,    void *aux) {
  /* Here, we define 'aux' as whether to compare ascending or descending */
  
  /* Obtain the two semaphores. */
  struct semaphore* sema_a = &list_entry(a, struct semaphore_elem, elem)->semaphore;
  struct semaphore* sema_b = &list_entry(b, struct semaphore_elem, elem)->semaphore;

  /* Obtain the biggest priority thread; firstly, obtain the largest LIST element. */
  struct list_elem* biggest_prithread_a_elem = list_max(&sema_a->waiters, thread_elem_cmp_priority, &cmp_less_than);
  struct list_elem* biggest_prithread_b_elem = list_max(&sema_b->waiters, thread_elem_cmp_priority, &cmp_less_than);

  /* Now, unwrap the list elements to get the threads. */
  struct thread* biggest_prithread_a = list_entry(biggest_prithread_a_elem, struct thread, elem);
  struct thread* biggest_prithread_b = list_entry(biggest_prithread_b_elem, struct thread, elem);

  return thread_cmp_priority(biggest_prithread_a, biggest_prithread_b, aux);
}

/* Atomically releases LOCK and waits for COND to be signaled by
   some other piece of code.  After COND is signaled, LOCK is
   reacquired before returning.  LOCK must be held before calling
   this function.

   The monitor implemented by this function is "Mesa" style, not
   "Hoare" style, that is, sending and receiving a signal are not
   an atomic operation.  Thus, typically the caller must recheck
   the condition after the wait completes and, if necessary, wait
   again.

   A given condition variable is associated with only a single
   lock, but one lock may be associated with any number of
   condition variables.  That is, there is a one-to-many mapping
   from locks to condition variables.

   This function may sleep, so it must not be called within an
   interrupt handler.  This function may be called with
   interrupts disabled, but interrupts will be turned back on if
   we need to sleep. */
void
cond_wait (struct condition *cond, struct lock *lock) 
{
  struct semaphore_elem waiter;

  ASSERT (cond != NULL);
  ASSERT (lock != NULL);
  ASSERT (!intr_context ());
  ASSERT (lock_held_by_current_thread (lock));
  
  sema_init (&waiter.semaphore, 0);
  list_elem_init(&waiter.elem);
  list_push_back(&cond->waiters,&waiter.elem);
  // list_insert_ordered (&cond->waiters, &waiter.elem,sema_elem_cmp_priority,&cmp_greater_than);
  lock_release (lock);
  sema_down (&waiter.semaphore);
  lock_acquire (lock);
}

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
 
  if (!list_empty (&cond->waiters)) {

    /* Get the top priority waiter. */

    struct list_elem* top_priority_waiter_sema_element = list_max(&cond->waiters, sema_elem_cmp_priority, &cmp_less_than);

    list_remove(top_priority_waiter_sema_element); /* Remove this element. */

    /* Unwrap the element. */

    struct semaphore_elem *top_priority_waiter_sema = list_entry (top_priority_waiter_sema_element,
                          struct semaphore_elem, elem);

    sema_up (&top_priority_waiter_sema->semaphore); 
  } 
}

/* Wakes up all threads, if any, waiting on COND (protected by
   LOCK).  LOCK must be held before calling this function.

   An interrupt handler cannot acquire a lock, so it does not
   make sense to try to signal a condition variable within an
   interrupt handler. */
void
cond_broadcast (struct condition *cond, struct lock *lock) 
{
  ASSERT (cond != NULL);
  ASSERT (lock != NULL);

  while (!list_empty (&cond->waiters))
    cond_signal (cond, lock);
}
