#ifndef USERPROG_PROCESS_H
#define USERPROG_PROCESS_H

#include <list.h>
#include <hash.h>
#include "threads/thread.h"
#include "vm/mmap.h"

#define PROC_SUCC (0)  /* Exit code for normal process termination. */
#define PROC_ERR (-1)  /* Exit code for erroneous process termination. */
#define FD_ERR (-1)    /* Returned when a file descriptor can't be used. */
#define FD_INIT (2)    /* Minimum valid file descriptor number, reserves 0 for
                          stdin and 1 for stdout. */

struct child_record;

/* A user process, wrapped around a kernel thread.  Only its own thread
   uses it, so it needs no lock. */
struct process
  {
    uint32_t *pagedir;            /* Process page directory. */
    struct thread *thread;        /* Process underlying kernel thread. */
    struct child_record *record;  /* Shared with the parent, null for root. */
    struct file *executable;      /* Kept open so it can't be written to. */
    struct hash fd_table;         /* Open files, keyed by file descriptor. */
    struct list children;         /* Records of children not waited for. */
    int next_fd;                  /* Next file descriptor to give out. */
    struct hash spt;              /* Supplemental page table, by upage. */
    struct mmap_table mmaps;      /* Memory mappings and the next mapid. */
    void *user_esp;               /* User esp saved on syscall entry. */
  };

void root_process_init (void);
tid_t process_execute (const char *cmd_line);
int process_wait (tid_t);
void process_exit (int) NO_RETURN;
void process_activate (void);
int process_fd_add (struct file *file);
struct file *process_fd_lookup_file (int fd);
struct file *process_fd_remove (int fd);

#endif /* userprog/process.h */
