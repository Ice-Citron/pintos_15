#ifndef USERPROG_PROCESS_H
#define USERPROG_PROCESS_H

#include <list.h>
#include <hash.h>
#include "threads/synch.h"
#include "threads/thread.h"

#define PROC_SUCC (0)  /* Exit code for normal process termination. */
#define PROC_ERR (-1)  /* Exit code for erroneous process termination. */
#define FD_ERR (-1)    /* Returned when a file descriptor can't be used. */
#define FD_INIT (2)    /* Minimum valid file descriptor number, reserves 0 for
                          stdin and 1 for stdout. */

/* A childs exit status which its parent keeps until it waits for it */
struct child_record
  {
    tid_t tid;                /* Thread identifier of the child */
    int exit_code;            /* Set by the child when it exits */
    struct semaphore exited;  /* Upped by the child when it exits */
    struct lock lock;         /* Protects ref_cnt. */
    int ref_cnt;              /* How many of parent and child still hold it. */
    struct list_elem elem;    /* Element in the parents children list */
  };

/* A user process, wrapped around a kernel thread. */
struct process
  {
    uint32_t *pagedir;      /* Process page directory. */
    struct thread *thread;  /* Process underlying kernel thread. */
    struct child_record *record;  /* Shared with the parent, null for root. */
    struct file *executable;      /* Kept open so it can't be written to. */
    struct hash fd_table;         /* File descriptor table for current process. */
    struct list children;         /* Records of children not waited for. */
    int next_fd;                  /* Next file descriptor to allocate 
                                     (monotonically increasing) */
  };

void root_process_init (void);
tid_t process_execute (const char *cmd_line);
int process_wait (tid_t);
void process_exit (int) NO_RETURN;
void process_activate (void);
int process_fd_add (struct file *file);
struct file *process_fd_get (int fd);
struct file *process_fd_remove (int fd);

#endif /* userprog/process.h */
