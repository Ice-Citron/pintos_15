#include "userprog/syscall.h"

static void syscall_handler (struct intr_frame *);

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
      break;
    case SYS_EXIT:
      /* Terminate this process. */
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