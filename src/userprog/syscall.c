#include "userprog/syscall.h"
#include "userprog/process.h"
#include "userprog/safemem.h"
#include "devices/input.h"
#include "devices/shutdown.h"
#include "threads/interrupt.h"
#include "threads/thread.h"
#include "filesys/file.h"
#include "filesys/filesys.h"
#include <debug.h>
#include <stdio.h>
#include <syscall-nr.h>
#include <stdbool.h>

/* Most arguments any system call takes. */
#define SYSCALL_MAX_ARGS 3

/* A system call: takes its arguments, already read from the user stack,
   and returns the value the user program finds in eax. */
typedef uint32_t syscall_func (const uint32_t *args);

/* How to run one system call. */
struct syscall
  {
    syscall_func *func;         /* Does the work. */
    int argc;                   /* Number of arguments it takes. */
  };

static void syscall_handler (struct intr_frame *);
static uint32_t syscall_halt (const uint32_t *args);
static uint32_t syscall_exit (const uint32_t *args);
static uint32_t syscall_exec (const uint32_t *args);
static uint32_t syscall_wait (const uint32_t *args);
static uint32_t syscall_create (const uint32_t *args);
static uint32_t syscall_remove (const uint32_t *args);
static uint32_t syscall_open (const uint32_t *args);
static uint32_t syscall_filesize (const uint32_t *args);
static uint32_t syscall_read (const uint32_t *args);
static uint32_t syscall_write (const uint32_t *args);
static uint32_t syscall_seek (const uint32_t *args);
static uint32_t syscall_tell (const uint32_t *args);
static uint32_t syscall_close (const uint32_t *args);

/* Every system call, indexed by its number (lib/syscall-nr.h). */
static const struct syscall syscalls[] =
  {
    [SYS_HALT] = {syscall_halt, 0},
    [SYS_EXIT] = {syscall_exit, 1},
    [SYS_EXEC] = {syscall_exec, 1},
    [SYS_WAIT] = {syscall_wait, 1},
    [SYS_CREATE] = {syscall_create, 2},
    [SYS_REMOVE] = {syscall_remove, 1},
    [SYS_OPEN] = {syscall_open, 1},
    [SYS_FILESIZE] = {syscall_filesize, 1},
    [SYS_READ] = {syscall_read, 3},
    [SYS_WRITE] = {syscall_write, 3},
    [SYS_SEEK] = {syscall_seek, 2},
    [SYS_TELL] = {syscall_tell, 1},
    [SYS_CLOSE] = {syscall_close, 1},
  };

/* Number of entries in SYSCALLS. */
#define SYSCALL_CNT (sizeof syscalls / sizeof *syscalls)

void
syscall_init (void)
{
  intr_register_int (0x30, 3, INTR_ON, syscall_handler, "syscall");
}

/* Reads the system call number and arguments from the user stack in F,
   runs the system call and puts its result in F's eax.  Kills the process
   if the number is unknown or any of it is not in user memory. */
static void
syscall_handler (struct intr_frame *f)
{
  uint32_t syscall_num;
  uint32_t args[SYSCALL_MAX_ARGS];
  const struct syscall *call;
  int i;

  /* All four bytes of the syscall number must be user memory, since they
     can straddle a page boundary. */
  if (!safemem_is_valid_buf (f->esp, sizeof syscall_num))
    {
      process_exit (PROC_ERR);
    }
  syscall_num = *(uint32_t *) f->esp;
  if (syscall_num >= SYSCALL_CNT || syscalls[syscall_num].func == NULL)
    {
      process_exit (PROC_ERR);
    }

  call = &syscalls[syscall_num];
  for (i = 0; i < call->argc; i++)
    {
      args[i] = safemem_read_syscall_arg (f, i);
    }
  f->eax = call->func (args);
}

/* halt (): shuts PintOS down. */
static uint32_t
syscall_halt (const uint32_t *args UNUSED)
{
  shutdown_power_off ();
}

/* exit (status): ends the current process, reporting STATUS to its
   parent. */
static uint32_t
syscall_exit (const uint32_t *args)
{
  process_exit ((int) args[0]);
}

/* exec (cmd_line): runs the program named in CMD_LINE, passing it the rest
   of the line as arguments.  Returns the new process's pid, or -1 if the
   program cannot be loaded.  Kills the caller if CMD_LINE is not a valid
   string. */
static uint32_t
syscall_exec (const uint32_t *args)
{
  const char *cmd_line = (const char *) args[0];

  if (!safemem_is_valid_string (cmd_line))
    {
      process_exit (PROC_ERR);
    }
  return process_execute (cmd_line);
}

/* wait (pid): waits for child process PID to exit and returns its exit
   status, or -1 if PID is not a child of the caller, was already waited
   for, or was killed by the kernel.  A pid is the child's tid. */
static uint32_t
syscall_wait (const uint32_t *args)
{
  return process_wait ((tid_t) args[0]);
}

/* create (file, initial_size): creates a file called FILE of INITIAL_SIZE
   bytes.  Returns true if successful, false otherwise. */
static uint32_t
syscall_create (const uint32_t *args)
{
  const char *file = (const char *) args[0];
  unsigned initial_size = (unsigned) args[1];

  if (!safemem_is_valid_string (file))
    {
      process_exit (PROC_ERR);
    }
  return filesys_create (file, initial_size);
}

/* remove (file): deletes the file called FILE.  Returns true if
   successful, false otherwise. */
static uint32_t
syscall_remove (const uint32_t *args)
{
  const char *file = (const char *) args[0];

  if (!safemem_is_valid_string (file))
    {
      process_exit (PROC_ERR);
    }
  return filesys_remove (file);
}

/* open (file): opens the file called FILE.  Returns its new file
   descriptor, or FD_ERR if it could not be opened. */
static uint32_t
syscall_open (const uint32_t *args)
{
  const char *file = (const char *) args[0];
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

/* filesize (fd): returns the size, in bytes, of the file open as FD, or
   FD_ERR if FD is not open. */
static uint32_t
syscall_filesize (const uint32_t *args)
{
  struct file *file = process_fd_get ((int) args[0]);

  if (file == NULL)
    {
      return FD_ERR;
    }
  return file_length (file);
}

/* read (fd, buffer, size): reads SIZE bytes from FD, or from the keyboard
   if FD is STDIN_FILENO, into BUFFER.  Returns the number of bytes read
   (0 at end of file), or FD_ERR if FD is not open.  Kills the caller if
   BUFFER is not writable user memory. */
static uint32_t
syscall_read (const uint32_t *args)
{
  int fd = (int) args[0];
  void *buffer = (void *) args[1];
  unsigned size = (unsigned) args[2];
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

/* write (fd, buffer, size): writes SIZE bytes from BUFFER to FD, or to the
   console if FD is STDOUT_FILENO.  Returns the number of bytes written,
   which may be less than SIZE at the end of a file, since files cannot
   grow, or FD_ERR if FD is not open.  Kills the caller if BUFFER is not
   user memory. */
static uint32_t
syscall_write (const uint32_t *args)
{
  int fd = (int) args[0];
  const void *buffer = (const void *) args[1];
  unsigned size = (unsigned) args[2];
  struct file *file;

  if (!safemem_is_valid_buf (buffer, size))
    {
      process_exit (PROC_ERR);
    }

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

/* seek (fd, position): makes POSITION, in bytes from the start of the
   file, the next byte to be read or written in open file FD.  Returns 0. */
static uint32_t
syscall_seek (const uint32_t *args)
{
  struct file *file = process_fd_get ((int) args[0]);
  unsigned position = (unsigned) args[1];

  if (file != NULL && position <= INT32_MAX)
    {
      file_seek (file, position);
    }
  return 0;
}

/* tell (fd): returns the position of the next byte to be read or written
   in open file FD, in bytes from the start of the file, or 0 if FD is not
   open. */
static uint32_t
syscall_tell (const uint32_t *args)
{
  struct file *file = process_fd_get ((int) args[0]);

  if (file == NULL)
    {
      return 0;
    }
  return file_tell (file);
}

/* close (fd): closes file descriptor FD.  Returns 0. */
static uint32_t
syscall_close (const uint32_t *args)
{
  file_close (process_fd_remove ((int) args[0]));
  return 0;
}
