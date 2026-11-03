#ifndef USERPROG_PROCESS_H
#define USERPROG_PROCESS_H

#include <list.h>
#include <hash.h>
#include "threads/synch.h"
#include "threads/thread.h"

#define PROC_SUCC (0)  /* Exit code for normal process termination. */
#define PROC_ERR (-1)  /* Exit code for erroneous process termination. */

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

/* A structure for a file entry */
struct file_entry
{
  int fd;                /* File descriptor number */
  struct file *file;     /* File being pointed to of file entry  */
  struct hash_elem elem; /* Hash element of a file entry */
};

/* Helper functions for process file descriptor table. */
bool process_fd_table_insert (struct hash *fd_table, int fd,
                              struct file *entry);
struct file *process_fd_table_get (struct hash *fd_table, int fd);
struct file *process_fd_table_delete (struct hash *fd_table, int fd);

/* A user process, wrapped around a kernel thread. */
struct process
  {
    uint32_t *pagedir;      /* Process page directory. */
    struct thread *thread;  /* Process underlying kernel thread. */
    struct child_record *record;  /* Shared with the parent, null for root. */
    struct file *executable;      /* Kept open so it can't be written to. */
    struct hash fd_table;         /* File descriptor table for current process. */
    struct list children;         /* Records of children not waited for. */
  };

void root_process_init (void);
tid_t process_execute (const char *cmd_line);
int process_wait (tid_t);
void process_exit (int) NO_RETURN;
void process_activate (void);

#endif /* userprog/process.h */
