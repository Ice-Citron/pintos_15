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
static tid_t syscall_exec (const char *cmd_line);
static int syscall_wait (tid_t tid);
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

/* Reads the system call number and arguments from the user stack in F,
   runs the system call and puts its result in F's eax. */
static void
syscall_handler (struct intr_frame *f)
{
  uint32_t syscall_num;
  uint32_t ret_code = 0;

  /* All four bytes of the syscall number must be user memory, since they
     can straddle a page boundary. */
  if (!safemem_is_valid_buf (f->esp, sizeof syscall_num))
    {
      process_exit (PROC_ERR);
    }
  syscall_num = *(uint32_t *) f->esp;

  /* Dispatch on the syscall number (see lib/syscall-nr.h). */
  switch (syscall_num)
    {
    case SYS_HALT:
      syscall_halt ();
      break;
    case SYS_EXIT:
      {
        int exit_code = (int) safemem_read_syscall_arg (f, 0);
        syscall_exit (exit_code);
      }
      break;
    case SYS_EXEC:
      {
        const char *cmd_line = (const char *) safemem_read_syscall_arg (f, 0);
        ret_code = syscall_exec (cmd_line);
      }
      break;
    case SYS_WAIT:
      {
        tid_t tid = (tid_t) safemem_read_syscall_arg (f, 0);
        ret_code = syscall_wait (tid);
      }
      break;
    case SYS_CREATE:
      {
        const char *file_name = (const char *) safemem_read_syscall_arg (f, 0);
        unsigned int initial_size = (unsigned) safemem_read_syscall_arg (f, 1);
        ret_code = syscall_create (file_name, initial_size);
      }
      break;
    case SYS_REMOVE:
      {
        const char *file_name = (const char *) safemem_read_syscall_arg (f, 0);
        ret_code = syscall_remove (file_name);
      }
      break;
    case SYS_OPEN:
      {
        const char *file_name = (const char *) safemem_read_syscall_arg (f, 0);
        ret_code = syscall_open (file_name);
      }
      break;
    case SYS_FILESIZE:
      {
        int fd = (int) safemem_read_syscall_arg (f, 0);
        ret_code = syscall_filesize (fd);
      }
      break;
    case SYS_READ:
      {
        int fd = (int) safemem_read_syscall_arg (f, 0);
        void *buffer = (void *) safemem_read_syscall_arg (f, 1);
        unsigned size = (unsigned) safemem_read_syscall_arg (f, 2);

        ret_code = syscall_read (fd, buffer, size);
      }
      break;
    case SYS_WRITE:
      {
        int fd = (int) safemem_read_syscall_arg (f, 0);
        const void *buffer = (const void *) safemem_read_syscall_arg (f, 1);
        unsigned size = (unsigned) safemem_read_syscall_arg (f, 2);

        ret_code = syscall_write (fd, buffer, size);
      }
      break;
    case SYS_SEEK:
      {
        int fd = (int) safemem_read_syscall_arg (f, 0);
        unsigned position = (unsigned) safemem_read_syscall_arg (f, 1);
        syscall_seek (fd, position);
      }
      break;
    case SYS_TELL:
      {
        int fd = (int) safemem_read_syscall_arg (f, 0);
        ret_code = syscall_tell (fd);
      }
      break;
    case SYS_CLOSE:
      {
        int fd = (int) safemem_read_syscall_arg (f, 0);
        syscall_close (fd);
      }
      break;
    default:
      process_exit (PROC_ERR);
      break;
    }

  /* The user program finds the result in eax. */
  f->eax = ret_code;
}

/* Shuts PintOS down. */
static void
syscall_halt (void)
{
  shutdown_power_off ();
}

/* Ends the current process, reporting EXIT_CODE to its parent. */
static void
syscall_exit (int exit_code)
{
  process_exit (exit_code);
}

/* Runs the program named in CMD_LINE, passing it the rest of the line as
   arguments.  Returns the new process's pid, or -1 if the program cannot
   be loaded.  Kills the caller if CMD_LINE is not a valid string. */
static tid_t
syscall_exec (const char *cmd_line)
{
  if (!safemem_is_valid_string (cmd_line))
    {
      process_exit (PROC_ERR);
    }
  return process_execute (cmd_line);
}

/* Waits for child process TID to exit and returns its exit status, or -1
   if TID is not a child of the caller, was already waited for, or was
   killed by the kernel. */
static int
syscall_wait (tid_t tid)
{
  return process_wait (tid);
}

/* Creates a file called FILE of INITIAL_SIZE bytes.  Returns true if
   successful, false otherwise. */
static bool
syscall_create (const char *file, unsigned initial_size)
{
  if (!safemem_is_valid_string (file))
    {
      process_exit (PROC_ERR);
    }
  return filesys_create (file, initial_size);
}

/* Deletes the file called FILE.  Returns true if successful, false
   otherwise. */
static bool
syscall_remove (const char *file)
{
  if (!safemem_is_valid_string (file))
    {
      process_exit (PROC_ERR);
    }
  return filesys_remove (file);
}

/* Opens file called FILE. Returns non-negative file descriptor on success, or
   FD_ERR if file could not be opened. */
static int
syscall_open (const char *file)
{
  struct file *opened_file;
  int fd;

  if (!safemem_is_valid_string (file))
    {
      process_exit (PROC_ERR);
    }
  opened_file = filesys_open (file);
  if (opened_file == NULL)
    {
      return FD_ERR;
    }
  fd = process_fd_add (opened_file);
  if (fd == FD_ERR)
    {
      file_close (opened_file);
    }
  return fd;
}

/* Returns the size, in bytes, of the file open as FD. */
static int
syscall_filesize (int fd)
{
  struct file *file;
  file = process_fd_get (fd);
  if (file == NULL)
    {
      return FD_ERR;
    }
  return file_length (file);
}

/* Reads SIZE bytes from file descriptor FD into BUFFER. Returns the number of
   bytes actually read (0 at end of file), or -1 if could not be read. */
static int
syscall_read (int fd, void *buffer, unsigned size)
{
  struct file *file;
  if (!safemem_is_writable_buf (buffer, size))
    {
      process_exit (PROC_ERR);
    }

  if (fd == STDIN_FILENO)
    {
      uint8_t *bytes = buffer;
      for (unsigned i = 0; i < size; i++)
        {
          bytes[i] = input_getc ();
        }
      return size;
    }

  file = process_fd_get (fd);
  if (file == NULL)
    {
      return FD_ERR;
    }
  return file_read (file, buffer, size);
}

/* Writes SIZE bytes from BUFFER to the open file FD, or to the console if
   FD is STDOUT_FILENO.  Returns the number of bytes written, which may be
   less than SIZE at the end of a file since files cannot grow, or FD_ERR
   if FD is not open. */
static uint32_t
syscall_write (int fd, const void *buffer, unsigned size)
{
  struct file *file;
  if (!safemem_is_valid_buf (buffer, size))
    {
      process_exit (PROC_ERR);
    }

  /* Write to console if filesystem requires it */
  if (fd == STDOUT_FILENO)
    {
      putbuf (buffer, size);
      return size;
    }

  file = process_fd_get (fd);
  if (file == NULL)
    {
      return FD_ERR;
    }
  return file_write (file, buffer, size);
}

/* Makes POSITION, in bytes from the start of the file, the next byte to be
   read or written in open file FD. */
static void
syscall_seek (int fd, unsigned position)
{
  struct file *file;
  file = process_fd_get (fd);
  if (file != NULL && position <= INT32_MAX)
    {
      file_seek (file, position);
    }
}

/* Returns position of the next byte to be read or written in open file FD,
   expressed in bytes from beginning of file. */
static unsigned
syscall_tell (int fd)
{
  struct file *file;
  file = process_fd_get (fd);
  if (file == NULL)
    {
      return 0;
    }
  return file_tell (file);
}

/* Closes file descriptor FD. */
static void
syscall_close (int fd)
{
  file_close (process_fd_remove (fd));
}
