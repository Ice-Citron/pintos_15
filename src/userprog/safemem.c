#include "userprog/safemem.h"
#include "threads/thread.h"
#include "threads/vaddr.h"
#include "userprog/pagedir.h"
#include "userprog/process.h"

static bool safemem_is_valid_addr (const void *addr);
static bool check_pages (const void *addr, unsigned size, bool writable);

/* Returns true if ADDR is a user address in a page that is mapped in the
   current process's page directory. */
static bool
safemem_is_valid_addr (const void *addr)
{
  return addr != NULL && is_user_vaddr (addr)
         && pagedir_get_page (thread_current ()->process->pagedir, addr)
            != NULL;
}

/* Returns true if all SIZE bytes starting at ADDR are mapped user memory. */
bool
safemem_is_valid_buf (const void *addr, unsigned size)
{
  return check_pages (addr, size, false);
}

/* Returns true if all SIZE bytes starting at ADDR are mapped user memory
   that the process may write to, so the kernel can safely fill them. */
bool
safemem_is_writable_buf (void *addr, unsigned size)
{
  return check_pages (addr, size, true);
}

/* Returns true if every page touched by the SIZE bytes starting at ADDR is
   mapped user memory and, if WRITABLE, is writable.  Memory is mapped a
   page at a time, so one address per page is checked. */
static bool
check_pages (const void *addr, unsigned size, bool writable)
{
  uint32_t *pd = thread_current ()->process->pagedir;
  uintptr_t first = (uintptr_t) addr;
  uintptr_t last = first + size - 1;
  uintptr_t page;

  if (size == 0)
    {
      return true;
    }

  /* A buffer that wraps past the top of memory can't be valid. */
  if (last < first)
    {
      return false;
    }

  for (page = (uintptr_t) pg_round_down (addr); page <= last; page += PGSIZE)
    {
      if (!safemem_is_valid_addr ((const void *) page)
          || (writable && !pagedir_is_writable (pd, (const void *) page)))
        {
          return false;
        }
    }
  return true;
}

/* Returns true if STR is a null-terminated string lying entirely in mapped
   user memory.  A page is only looked up when the string first enters it. */
bool
safemem_is_valid_string (const char *str)
{
  if (!safemem_is_valid_addr (str))
    {
      return false;
    }
  while (*str != '\0')
    {
      str++;
      if (pg_ofs (str) == 0 && !safemem_is_valid_addr (str))
        {
          return false;
        }
    }
  return true;
}

/* Returns argument ARGNUM (counting from 0) of the system call in F.  The
   syscall number is the word at the user stack pointer and the arguments
   are the words above it (PintOS manual, section 4.5.2).  Kills the process
   if the argument is not in user memory. */
uint32_t
safemem_read_syscall_arg (struct intr_frame *f, unsigned argnum)
{
  uint32_t *arg = (uint32_t *) f->esp + 1 + argnum;

  if (!safemem_is_valid_buf (arg, sizeof *arg))
    {
      process_exit (PROC_ERR);
    }
  return *arg;
}
