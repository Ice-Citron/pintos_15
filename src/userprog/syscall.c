#include "userprog/syscall.h"
#include "userprog/process.h"
#include "userprog/safemem.h"
#include "devices/input.h"
#include "devices/shutdown.h"
#include "threads/interrupt.h"
#include "threads/thread.h"
#include "filesys/file.h"
#include "filesys/filesys.h"
#include <stdio.h>
#include <syscall-nr.h>
#include <stdbool.h>

static void syscall_handler (struct intr_frame *);
static void syscall_halt (void);
static void syscall_exit (int exit_code);
static bool syscall_create (const char *file, unsigned initial_size);
static bool syscall_remove (const char *file);
static int syscall_open (const char *file);
static int syscall_filesize (int fd);
static int syscall_read (int fd, void *buffer, unsigned size);
static uint32_t syscall_write (int fd, const void *buffer, unsigned size);
static void syscall_seek (int fd, unsigned position);
static unsigned syscall_tell (int fd);
static void syscall_close (int fd);

void
syscall_init (void)
{
  intr_register_int (0x30, 3, INTR_ON, syscall_handler, "syscall");
}

static void
syscall_handler (struct intr_frame *f)
{
  /* Syscall arguments are passed to the handler on an interrupt frame (f). */

  /* Check that stack pointer (f->esp) points to a valid address
  If it does not, handle it SOMEHOW */

  if (!safemem_is_valid_addr (f->esp))
    process_exit (-1);

  /* First, read the syscall number at the stack pointer (f->esp). */
  uint32_t *syscall_num = (uint32_t *)f->esp;

  /* Store return code */
  uint32_t ret_code = 0x0;

  /* Then, pass these to the appropriate function (syscall numbers are defined
   * in lib/syscall-nr.h). */
  switch (*syscall_num)
    {
    /* Next, read the arguments above the stack pointer. */
    case SYS_HALT:
      /* Halt the operating system. */
      syscall_halt ();
      break;
    case SYS_EXIT:
      /* Terminate this process. */
      {
        int exit_code = (int) safemem_read_syscall_arg (f, 0);
        syscall_exit (exit_code);
      }
      break;
    case SYS_EXEC:
      /* Start another process. */
      break;
    case SYS_WAIT:
      /* Wait for a child process to die. */
      break;
    case SYS_CREATE:
      /* Create a file. */
      {
        const char *file_name = (const char *) safemem_read_syscall_arg (f, 0);
        unsigned int initial_size = (unsigned) safemem_read_syscall_arg (f, 1);
        ret_code = syscall_create (file_name, initial_size);
      }
      break;
    case SYS_REMOVE:
      /* Delete a file. */
      {
        const char *file_name = (const char *) safemem_read_syscall_arg (f, 0);
        ret_code = syscall_remove (file_name);
      }
      break;
    case SYS_OPEN:
      /* Open a file. */
      {
        const char *file_name = (const char *) safemem_read_syscall_arg (f, 0);
        ret_code = syscall_open (file_name);
      }
      break;
    case SYS_FILESIZE:
      /* Obtain a file's size. */
      break;
    case SYS_READ:
      /* Read from a file. */
      {
        int fd = (int) safemem_read_syscall_arg (f, 0);
        void *buffer = (void *) safemem_read_syscall_arg (f, 1);
        unsigned size = (unsigned) safemem_read_syscall_arg (f, 2);

        ret_code = syscall_read (fd, buffer, size);
      }
      break;
    case SYS_WRITE:
      /* Write to a file. */
      {
        int fd = (int) safemem_read_syscall_arg (f, 0);
        const void *buffer = (const void *) safemem_read_syscall_arg (f, 1);
        unsigned size = (unsigned) safemem_read_syscall_arg (f, 2);

        ret_code = syscall_write (fd, buffer, size);
      }
      break;
    case SYS_SEEK:
      /* Change position in a file. */
      break;
    case SYS_TELL:
      /* Report current position in a file. */
      break;
    case SYS_CLOSE:
      /* Close a file. */
      {
        int file_descriptor = (int) safemem_read_syscall_arg (f, 0);
        syscall_close (file_descriptor);
      }
      break;
    default:
      process_exit (PROC_ERR);
      break;
    }

  /* Finally, return the result to f->eax. */
  f->eax = ret_code;
}

static void
syscall_halt (void)
{
  shutdown_power_off ();
}

static void
syscall_exit (int exit_code)
{
  process_exit (exit_code);
}

/* Creates a file called FILE of INITIAL_SIZE bytes. Return true if successful, 
   false otherwise. */
static bool 
syscall_create (const char *file, unsigned initial_size)
{
  if (!safemem_is_valid_string ((char *) file))
    process_exit (PROC_ERR);
  return filesys_create (file, initial_size);
}

/* Deletes file called FILE. Return true if successful, false otherwise. */
static bool 
syscall_remove (const char *file)
{
  if (!safemem_is_valid_string ((char *) file))
    process_exit (PROC_ERR);
  return filesys_remove (file);
}

/* Opens file called FILE. Returns non-negative file descriptor on success, or
   FD_ERR if file could not be opened. */
static int 
syscall_open (const char *file)
{
  struct file *opened_file;
  int fd;

  if (!safemem_is_valid_string ((char *) file)) 
    process_exit (PROC_ERR);
  opened_file = filesys_open (file);
  if (opened_file == NULL)
    return FD_ERR;
  fd = process_fd_table_add (thread_current ()->process, opened_file);
  if (fd == FD_ERR)
    file_close (opened_file);
  return fd;
}

static int
syscall_filesize (int fd)
{
  return -1;
}

/* Reads SIZE bytes from file descriptor FD into BUFFER. Returns the number of
   bytes actually read (0 at end of file), or -1 if could not be read. */
static int
syscall_read (int fd, void *buffer, unsigned size)
{
  struct file *file;
  if (!safemem_is_valid_buf (buffer, size))
    process_exit (PROC_ERR);

  if (fd == STDIN_FILENO)
    {
      uint8_t *bytes = buffer;
      for (unsigned i = 0; i < size; i++)
        bytes[i] = input_getc ();
      return size;
    }
  
  /* Get FD's file from process' file descriptor table */
  file = process_fd_table_get (&thread_current ()->process->fd_table, fd);
  if (file == NULL)
    return FD_ERR;
  return file_read (file, buffer, size);}

/*
Syscall handler that writes size bytes from buffer to the open file fd.

Returns the number of bytes actually written, which may be less than size
if some bytes could not be written.

Writing past end-of-file would normally extend the file,
but file growth is not implemented by the basic file system.
The expected behaviour is to write as many bytes as possible up to
end-of-file and return the actual number written,
or 0 if no bytes could be written at all.
*/
static uint32_t
syscall_write (int fd, const void *buffer, unsigned size)
{
  struct file *file;
  if (!safemem_is_valid_buf ((void *) buffer, size))
    process_exit (PROC_ERR);

  /* Write to console if filesystem requires it */
  if (fd == STDOUT_FILENO)
    {
      putbuf (buffer, size);
      return size;
    }
  
  /* Get FD's file from process' file descriptor table */
  file = process_fd_table_get (&thread_current ()->process->fd_table, fd);
  if (file == NULL)
    return 0;
  return file_write (file, buffer, size);
}

static void
syscall_seek (int fd, unsigned position)
{
}

static unsigned
syscall_tell (int fd)
{
  return 0;
}

/* Closes file descriptor FD and removes file from current process' file 
   descriptor table */
static void
syscall_close (int fd)
{
  struct file *file;
  file = process_fd_table_delete (&thread_current ()->process->fd_table, fd);
  file_close (file);
}