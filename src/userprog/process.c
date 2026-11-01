#include "userprog/process.h"
#include <debug.h>
#include <inttypes.h>
#include <round.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "userprog/gdt.h"
#include "userprog/pagedir.h"
#include "userprog/tss.h"
#include "filesys/directory.h"
#include "filesys/file.h"
#include "filesys/filesys.h"
#include "threads/flags.h"
#include "threads/init.h"
#include "threads/interrupt.h"
#include "threads/malloc.h"
#include "threads/palloc.h"
#include "threads/thread.h"
#include "threads/vaddr.h"

/* Passed from process_execute() to start_process().  It lives on the
   parent's stack, which is safe because the parent waits until the child
   has loaded. */
struct process_args
  {
    char *cmd_line;               /* Page holding a copy of the command line. */
    struct child_record *record;  /* Where the new process reports its exit. */
    struct semaphore loaded;      /* Upped by the child once it has loaded. */
    bool load_success;            /* Whether the child loaded. */
  };

static bool process_init (struct thread *t, struct child_record *record);
static thread_func start_process NO_RETURN;
static bool load (const char *cmdline, void (**eip) (void), void **esp);
static bool push (void **esp, const void *data, size_t size);
static bool push_arguments (void **esp, const char *line, size_t size);
static void child_record_release (struct child_record *record);

/* Initialises the main thread as a minimal root OS process.
   Note that when this thread/process terminates, the OS will shutdown. */
void
root_process_init (void)
{
  struct thread *cur = thread_current ();
  bool success = process_init (cur, NULL);
  
  if (!success)
    PANIC("Unable to initialise root OS process.");
}

/* Starts a new thread running a user program loaded from CMD_LINE.
   The new thread may be scheduled (and may even exit) before process_execute() returns.
   Returns the new process's thread id, or TID_ERROR if CMD_LINE has no
   program name, the thread cannot be created or the program fails to
   load. */
tid_t
process_execute (const char *cmd_line) 
{
  struct process_args args;
  struct child_record *record;
  tid_t tid = TID_ERROR;

  /* The first word is the program name, so skip any spaces before it */
  cmd_line += strspn (cmd_line, " ");
  if (*cmd_line == '\0')
    return TID_ERROR;

  /* Make a copy of CMD_LINE.
     Otherwise there's a race between the caller and load(). */
  args.cmd_line = palloc_get_page (0);
  record = malloc (sizeof *record);
  if (args.cmd_line != NULL && record != NULL)
    {
      strlcpy (args.cmd_line, cmd_line, PGSIZE);
      args.record = record;
      sema_init (&args.loaded, 0);
      sema_init (&record->exited, 0);
      lock_init (&record->lock);
      record->ref_cnt = 2;
      list_elem_init (&record->elem);

      /* Create a new thread to execute CMD_LINE */
      tid = thread_create (cmd_line, PRI_DEFAULT, start_process, &args);
    }

  if (tid == TID_ERROR)
    free (record);
  else
    {
      /* Exec has to report whether the program loaded, so wait for the
         child to find out. */
      sema_down (&args.loaded);
      if (args.load_success)
        {
          /* The child may have exited already, but its record outlives it */
          record->tid = tid;
          list_push_back (&thread_current ()->process->children,
                          &record->elem);
        }
      else
        {
          child_record_release (record);
          tid = TID_ERROR;
        }
    }

  /* The child is done with the command line once it has loaded. */
  palloc_free_page (args.cmd_line);
  return tid;
}

/* Basic initialisation of a process running on thread T. */
static bool 
process_init (struct thread *t, struct child_record *record)
{
  /* Create process struct to store process meta-data */
  struct process *process = malloc (sizeof(struct process));
  if (process == NULL)
    {
      /* This process never gets as far as process_exit (), so let go of
         its record here. */
      if (record != NULL)
        child_record_release (record);
      return false;
    }

  /* Initialise process attributes. */
  process->pagedir = NULL;
  process->thread = t;
  process->record = record;
  list_init (&process->children);
  
  /* Link thread to its process. */
  t->process = process;
  return true;
}

/* A thread function that loads a user process and starts it
   running. */
static void
start_process (void *args_)
{
  struct thread *cur = thread_current ();
  struct process_args *args = args_;
  struct intr_frame if_;
  char *program, *p;
  size_t size;
  bool success;
  
  /* Initialise process. */
  success = process_init (cur, args->record);

  if (success)
    {
    /* Turn every space in the command line into a null so that it is a
      run of words. The first is the program so name the thread after it
      for the exit message. */
      program = args->cmd_line;
      for (p = program; *p != '\0'; p++)
        if (*p == ' ')
          *p = '\0';
      size = p - program + 1;
      strlcpy (cur->name, program, sizeof cur->name);

      /* Initialize interrupt frame and load executable. */
      memset (&if_, 0, sizeof if_);
      if_.gs = if_.fs = if_.es = if_.ds = if_.ss = SEL_UDSEG;
      if_.cs = SEL_UCSEG;
      if_.eflags = FLAG_IF | FLAG_MBS;
      success = load (program, &if_.eip, &if_.esp)
                && push_arguments (&if_.esp, program, size);
    }

  /* Tell the parent whether the load worked.  ARGS is on the parent's
     stack and the parent carries on as soon as it wakes, so this is the
     last use of it. */
  args->load_success = success;
  sema_up (&args->loaded);
  
  /* If load failed, terminate the process. */
  if (!success) 
    process_exit (PROC_ERR);

  /* Start the user process by simulating a return from an
     interrupt, implemented by intr_exit (in
     threads/intr-stubs.S).  Because intr_exit takes all of its
     arguments on the stack in the form of a `struct intr_frame',
     we just point the stack pointer (%esp) to our stack frame
     and jump to it. */
  asm volatile ("movl %0, %%esp; jmp intr_exit" : : "g" (&if_) : "memory");
  NOT_REACHED ();
}

/* Pushes SIZE bytes at DATA onto the user stack at *ESP. Returns false if
   there is no room left in the stack page. */
static bool
push (void **esp, const void *data, size_t size)
{
  uint8_t *bottom = (uint8_t *) PHYS_BASE - PGSIZE;
  if (size > (size_t) ((uint8_t *) *esp - bottom))
    return false;
  *esp = (uint8_t *) *esp - size;
  memcpy (*esp, data, size);
  return true;
}

/* Sets up the user stack at *ESP for the command line LINE which is SIZE
   bytes long with a null for every space:pushes the words, argv, argc and a
   fake return address. Returns false if they don't fit in the stack page */
static bool
push_arguments (void **esp, const char *line, size_t size)
{
  void *null = NULL;
  const char *word, *first;
  void *argv;
  int argc = 0;

  /* The line alr holds the words in order so it goes on as it is */
  if (!push (esp, line, size))
    return false;
  first = *esp;

  /* Align the stack, then push argv[argc] and the pointers to the words,
   * last word first, so that argv[0] is at the lowest address. */
  *esp = (void *) ROUND_DOWN ((uintptr_t) *esp, sizeof (uint32_t));
  if (!push (esp, &null, sizeof null))
    return false;
  for (word = (const char *) PHYS_BASE - 1; word >= first; word--)
    if (*word != '\0' && (word == first || word[-1] == '\0'))
      {
        if (!push (esp, &word, sizeof word))
          return false;
        argc++;
      }
  argv = *esp;
  return push (esp, &argv, sizeof argv) && push (esp, &argc, sizeof argc)
         && push (esp, &null, sizeof null);
}

/* Waits for thread TID to die and returns its exit status. 
 * If it was terminated by the kernel (i.e. killed due to an exception), 
 * returns -1.  
 * If TID is invalid or if it was not a child of the calling process, or if 
 * process_wait() has already been successfully called for the given TID, 
 * returns -1 immediately, without waiting. */
int
process_wait (tid_t child_tid)
{
  struct list *children = &thread_current ()->process->children;
  struct list_elem *e;

  for (e = list_begin (children); e != list_end (children); e = list_next (e))
    {
      struct child_record *child = list_entry (e, struct child_record, elem);
      int exit_code;

      if (child->tid != child_tid)
        continue;

      /* Each child can only be waited for once */
      sema_down (&child->exited);
      exit_code = child->exit_code;
      list_remove (e);
      child_record_release (child);
      return exit_code;
    }
  return PROC_ERR;
}

/* Drops one hold on RECORD, freeing it if neither the parent nor the child
   holds it any more. */
static void
child_record_release (struct child_record *record)
{
  bool last;

  lock_acquire (&record->lock);
  last = --record->ref_cnt == 0;
  lock_release (&record->lock);

  if (last)
    free (record);
}

/* Free the current process's resources and then exit the underlying thread. */
void
process_exit (int exit_code)
{
  struct process *cur = thread_current ()->process;
  uint32_t *pd;
  
  /* Clean up all process memory footprint, if it exists. */
  if (cur != NULL)
    {  
      struct child_record *record = cur->record;

      /* Let go of the records of children that were never waited for. Each
         is freed here or by its child, whichever lets go last. */
      while (!list_empty (&cur->children))
        child_record_release (list_entry (list_pop_front (&cur->children),
                                          struct child_record, elem));

      /* Destroy the current process's page directory and switch back
         to the kernel-only page directory. */
      pd = cur->pagedir;
      if (pd != NULL) 
        {
          /* Correct ordering here is crucial.  We must set
             cur->pagedir to NULL before switching page directories,
             so that a timer interrupt can't switch back to the
             process page directory.  We must activate the base page
             directory before destroying the process's page
             directory, or our active page directory will be one
             that's been freed (and cleared). */
          cur->pagedir = NULL;
          pagedir_activate (NULL);
          pagedir_destroy (pd);
        }
  
      /* Finally, destroy the current process's struct */
      thread_current ()->process = NULL;
      free(cur);

      /* Report the exit.  Wake the parent last, so that it only resumes once
      this process has let go of everything. */
      if (record != NULL)
        {
          printf ("%s: exit(%d)\n", thread_name (), exit_code);
          record->exit_code = exit_code;
          sema_up (&record->exited);
          child_record_release (record);
        }
    }
    
  /* Now terminate the thread underlying this process. */
  thread_exit();
  NOT_REACHED ();
}

/* Sets up the CPU for running user code in the current thread.
   This function is called on every context switch. */
void
process_activate (void)
{
  struct process *cur = thread_current ()->process;
  
  /* Activate thread's page tables if running an active process. */
  if (cur != NULL)  
    pagedir_activate (cur->pagedir);

  /* Set thread's kernel stack for use in processing
     interrupts. */
  tss_update ();
}

/* We load ELF binaries.  The following definitions are taken
   from the ELF specification, [ELF1], more-or-less verbatim.  */

/* ELF types.  See [ELF1] 1-2. */
typedef uint32_t Elf32_Word, Elf32_Addr, Elf32_Off;
typedef uint16_t Elf32_Half;

/* For use with ELF types in printf(). */
#define PE32Wx PRIx32   /* Print Elf32_Word in hexadecimal. */
#define PE32Ax PRIx32   /* Print Elf32_Addr in hexadecimal. */
#define PE32Ox PRIx32   /* Print Elf32_Off in hexadecimal. */
#define PE32Hx PRIx16   /* Print Elf32_Half in hexadecimal. */

/* Executable header.  See [ELF1] 1-4 to 1-8.
   This appears at the very beginning of an ELF binary. */
struct Elf32_Ehdr
  {
    unsigned char e_ident[16];
    Elf32_Half    e_type;
    Elf32_Half    e_machine;
    Elf32_Word    e_version;
    Elf32_Addr    e_entry;
    Elf32_Off     e_phoff;
    Elf32_Off     e_shoff;
    Elf32_Word    e_flags;
    Elf32_Half    e_ehsize;
    Elf32_Half    e_phentsize;
    Elf32_Half    e_phnum;
    Elf32_Half    e_shentsize;
    Elf32_Half    e_shnum;
    Elf32_Half    e_shstrndx;
  };

/* Program header.  See [ELF1] 2-2 to 2-4.
   There are e_phnum of these, starting at file offset e_phoff
   (see [ELF1] 1-6). */
struct Elf32_Phdr
  {
    Elf32_Word p_type;
    Elf32_Off  p_offset;
    Elf32_Addr p_vaddr;
    Elf32_Addr p_paddr;
    Elf32_Word p_filesz;
    Elf32_Word p_memsz;
    Elf32_Word p_flags;
    Elf32_Word p_align;
  };

/* Values for p_type.  See [ELF1] 2-3. */
#define PT_NULL    0            /* Ignore. */
#define PT_LOAD    1            /* Loadable segment. */
#define PT_DYNAMIC 2            /* Dynamic linking info. */
#define PT_INTERP  3            /* Name of dynamic loader. */
#define PT_NOTE    4            /* Auxiliary info. */
#define PT_SHLIB   5            /* Reserved. */
#define PT_PHDR    6            /* Program header table. */
#define PT_STACK   0x6474e551   /* Stack segment. */

/* Flags for p_flags.  See [ELF3] 2-3 and 2-4. */
#define PF_X 1          /* Executable. */
#define PF_W 2          /* Writable. */
#define PF_R 4          /* Readable. */

static bool setup_stack (void **esp);
static bool validate_segment (const struct Elf32_Phdr *, struct file *);
static bool load_segment (struct file *file, off_t ofs, uint8_t *upage,
                          uint32_t read_bytes, uint32_t zero_bytes,
                          bool writable);

/* Loads an ELF executable from FILE_NAME into the current thread.
   Stores the executable's entry point into *EIP
   and its initial stack pointer into *ESP.
   Returns true if successful, false otherwise. */
bool
load (const char *file_name, void (**eip) (void), void **esp) 
{
  struct process *cur = thread_current ()->process;
  struct Elf32_Ehdr ehdr;
  struct file *file = NULL;
  off_t file_ofs;
  bool success = false;
  int i;

  /* Allocate and activate page directory. */
  cur->pagedir = pagedir_create ();
  if (cur->pagedir == NULL) 
    goto done;
  process_activate ();

  /* Open executable file. */
  file = filesys_open (file_name);
  if (file == NULL) 
    {
      printf ("load: %s: open failed\n", file_name);
      goto done; 
    }

  /* Read and verify executable header. */
  if (file_read (file, &ehdr, sizeof ehdr) != sizeof ehdr
      || memcmp (ehdr.e_ident, "\177ELF\1\1\1", 7)
      || ehdr.e_type != 2
      || ehdr.e_machine != 3
      || ehdr.e_version != 1
      || ehdr.e_phentsize != sizeof (struct Elf32_Phdr)
      || ehdr.e_phnum > 1024) 
    {
      printf ("load: %s: error loading executable\n", file_name);
      goto done; 
    }

  /* Read program headers. */
  file_ofs = ehdr.e_phoff;
  for (i = 0; i < ehdr.e_phnum; i++) 
    {
      struct Elf32_Phdr phdr;

      if (file_ofs < 0 || file_ofs > file_length (file))
        goto done;
      file_seek (file, file_ofs);

      if (file_read (file, &phdr, sizeof phdr) != sizeof phdr)
        goto done;
      file_ofs += sizeof phdr;
      switch (phdr.p_type) 
        {
        case PT_NULL:
        case PT_NOTE:
        case PT_PHDR:
        case PT_STACK:
        default:
          /* Ignore this segment. */
          break;
        case PT_DYNAMIC:
        case PT_INTERP:
        case PT_SHLIB:
          goto done;
        case PT_LOAD:
          if (validate_segment (&phdr, file)) 
            {
              bool writable = (phdr.p_flags & PF_W) != 0;
              uint32_t file_page = phdr.p_offset & ~PGMASK;
              uint32_t mem_page = phdr.p_vaddr & ~PGMASK;
              uint32_t page_offset = phdr.p_vaddr & PGMASK;
              uint32_t read_bytes, zero_bytes;
              if (phdr.p_filesz > 0)
                {
                  /* Normal segment.
                     Read initial part from disk and zero the rest. */
                  read_bytes = page_offset + phdr.p_filesz;
                  zero_bytes = (ROUND_UP (page_offset + phdr.p_memsz, PGSIZE)
                                - read_bytes);
                }
              else 
                {
                  /* Entirely zero.
                     Don't read anything from disk. */
                  read_bytes = 0;
                  zero_bytes = ROUND_UP (page_offset + phdr.p_memsz, PGSIZE);
                }
              if (!load_segment (file, file_page, (void *) mem_page,
                                 read_bytes, zero_bytes, writable))
                goto done;
            }
          else
            goto done;
          break;
        }
    }

  /* Set up stack. */
  if (!setup_stack (esp))
    goto done;

  /* Start address. */
  *eip = (void (*) (void)) ehdr.e_entry;

  success = true;

 done:
  /* We arrive here whether the load is successful or not. */
  file_close (file);
  return success;
}

/* load() helpers. */

static bool install_page (void *upage, void *kpage, bool writable);

/* Checks whether PHDR describes a valid, loadable segment in
   FILE and returns true if so, false otherwise. */
static bool
validate_segment (const struct Elf32_Phdr *phdr, struct file *file) 
{
  /* p_offset and p_vaddr must have the same page offset. */
  if ((phdr->p_offset & PGMASK) != (phdr->p_vaddr & PGMASK)) 
    return false; 

  /* p_offset must point within FILE. */
  if (phdr->p_offset > (Elf32_Off) file_length (file)) 
    return false;

  /* p_memsz must be at least as big as p_filesz. */
  if (phdr->p_memsz < phdr->p_filesz) 
    return false; 

  /* The segment must not be empty. */
  if (phdr->p_memsz == 0)
    return false;
  
  /* The virtual memory region must both start and end within the
     user address space range. */
  if (!is_user_vaddr ((void *) phdr->p_vaddr))
    return false;
  if (!is_user_vaddr ((void *) (phdr->p_vaddr + phdr->p_memsz)))
    return false;

  /* The region cannot "wrap around" across the kernel virtual
     address space. */
  if (phdr->p_vaddr + phdr->p_memsz < phdr->p_vaddr)
    return false;

  /* Disallow mapping page 0.
     Not only is it a bad idea to map page 0, but if we allowed
     it then user code that passed a null pointer to system calls
     could quite likely panic the kernel by way of null pointer
     assertions in memcpy(), etc. */
  if (phdr->p_vaddr < PGSIZE)
    return false;

  /* It's okay. */
  return true;
}

/* Loads a segment starting at offset OFS in FILE at address
   UPAGE.  In total, READ_BYTES + ZERO_BYTES bytes of virtual
   memory are initialized, as follows:

        - READ_BYTES bytes at UPAGE must be read from FILE
          starting at offset OFS.

        - ZERO_BYTES bytes at UPAGE + READ_BYTES must be zeroed.

   The pages initialized by this function must be writable by the
   user process if WRITABLE is true, read-only otherwise.

   Return true if successful, false if a memory allocation error
   or disk read error occurs. */
static bool
load_segment (struct file *file, off_t ofs, uint8_t *upage,
              uint32_t read_bytes, uint32_t zero_bytes, bool writable) 
{
  ASSERT ((read_bytes + zero_bytes) % PGSIZE == 0);
  ASSERT (pg_ofs (upage) == 0);
  ASSERT (ofs % PGSIZE == 0);

  file_seek (file, ofs);
  while (read_bytes > 0 || zero_bytes > 0) 
    {
      /* Calculate how to fill this page.
         We will read PAGE_READ_BYTES bytes from FILE
         and zero the final PAGE_ZERO_BYTES bytes. */
      size_t page_read_bytes = read_bytes < PGSIZE ? read_bytes : PGSIZE;
      size_t page_zero_bytes = PGSIZE - page_read_bytes;
        
      /* Get a new page of memory. */
      uint8_t *kpage = palloc_get_page (PAL_USER);
      if (kpage == NULL){
        return false;
      }
        
      /* Add the page to the process's address space. */
      if (!install_page (upage, kpage, writable)) 
      {
        palloc_free_page (kpage);
        return false; 
      }     

      /* Load data into the page. */
      if (file_read (file, kpage, page_read_bytes) != (int) page_read_bytes){
        return false; 
      }
      memset (kpage + page_read_bytes, 0, page_zero_bytes);

      /* Advance. */
      read_bytes -= page_read_bytes;
      zero_bytes -= page_zero_bytes;
      upage += PGSIZE;
    }
  return true;
}

/* Create a minimal stack by mapping a zeroed page at the top of
   user virtual memory. */
static bool
setup_stack (void **esp) 
{
  uint8_t *kpage;
  bool success = false;

  kpage = palloc_get_page (PAL_USER | PAL_ZERO);
  if (kpage != NULL) 
    {
      success = install_page (((uint8_t *) PHYS_BASE) - PGSIZE, kpage, true);
      if (success)
        *esp = PHYS_BASE;
      else
        palloc_free_page (kpage);
    }
  return success;
}

/* Adds a mapping from user virtual address UPAGE to kernel
   virtual address KPAGE to the page table.
   If WRITABLE is true, the user process may modify the page;
   otherwise, it is read-only.
   UPAGE must not already be mapped.
   KPAGE should probably be a page obtained from the user pool
   with palloc_get_page().
   Returns true on success, false if UPAGE is already mapped or
   if memory allocation fails. */
static bool
install_page (void *upage, void *kpage, bool writable)
{
  struct process *cur = thread_current ()->process;

  /* Verify that there's not already a page at that virtual
     address, then map our page there. */
  return (pagedir_get_page (cur->pagedir, upage) == NULL
          && pagedir_set_page (cur->pagedir, upage, kpage, writable));
}
