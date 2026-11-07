#include "userprog/syscall.h"
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
      syscall_exit (safemem_read_syscall_arg (f, 0));
      break;
    case SYS_EXEC:
      /* Start another process. */
      break;
    case SYS_WAIT:
      /* Wait for a child process to die. */
      break;
    case SYS_CREATE:
      /* Create a file. */
      break;
    case SYS_REMOVE:
      /* Delete a file. */
      break;
    case SYS_OPEN:
      /* Open a file. */
      break;
    case SYS_FILESIZE:
      /* Obtain a file's size. */
      break;
    case SYS_READ:
      /* Read from a file. */
      break;
    case SYS_WRITE:
      /* Write to a file. */
      {
        int fd = (int)safemem_read_syscall_arg (f, 0);
        const void *buffer = (const void *)safemem_read_syscall_arg (f, 1);
        unsigned size = (unsigned)safemem_read_syscall_arg (f, 2);

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

static bool syscall_create (const char *file, unsigned initial_size)
{
  return false;
}

static bool syscall_remove (const char *file)
{
  return false;
}

static int syscall_open (const char *file)
{
  return -1;
}

static int syscall_filesize (int fd)
{
  return -1;
}

static int syscall_read (int fd, void *buffer, unsigned size)
{
  return -1;
}

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
  /* Track the number of bytes written */
  uint32_t bytes_written = 0;
  /* Check if valid address*/
  if (!safemem_is_valid_addr (buffer))
    process_exit (-1);

  /* Write to console if the filesystem requires it */
  if (fd == STDOUT_FILENO)
    {
      putbuf (buffer, size);
      return size;
    }

  /* TODO: write bytes to file */
  return bytes_written;
}

static void syscall_seek (int fd, unsigned position)
{
}

static unsigned syscall_tell (int fd)
{
  return 0;
}

static void syscall_close (int fd)
{
}