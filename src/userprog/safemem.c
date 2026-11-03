#include "userprog/safemem.h"

/*
Returns if the byte at address specified at ADDR is a valid address,
by checking if it is non-NULL, is a user address, and is allocated to a page.
*/
bool
safemem_is_valid_addr (void *addr)
{
  return (addr != NULL) && is_user_vaddr (addr)
         && pagedir_get_page (thread_current ()->process->pagedir, addr)
                != NULL;
}

/*
Returns if every byte the block of memory from [ADDR, ADDR + SIZE] is all
valid.
*/
bool
safemem_is_valid_buf (void *addr, unsigned size)
{

  /* If the size of the buffer is 0, it is valid anyways. */
  if (size == 0)
    return true;

  const uint8_t *low = pg_round_down (addr);
  const uint8_t *high = addr + size - 1;
  for (uint8_t *cur = low; cur <= high; cur++)
    {
      if (!safemem_is_valid_addr (cur))
        return false;
    }
  return true;
}

/*
Returns if an allocated string starting at str is valid.
*/
bool
safemem_is_valid_string (char *str)
{
  /* Repeatedly check if the pointer to the
  current character is valid. */
  while (safemem_is_valid_addr (str))
    {
      /* Once a valid terminator is reached,
      return true. */
      if (*str == '\0')
        return true;
      str++;
    }
  /* The loop exits when any part of the string
  is not a valid address. */
  return false;
}

/*
Reads the (N + 1)th word (argument) after f->esp.
This is the Nth (starting from 0th) argument of the syscall;

§4.5.2 System Call Details
The system call number is in the 32-bit word at the caller's stack pointer,
the first argument is in the 32-bit word at the next higher address, and so on.
*/
uint32_t
safemem_read_syscall_arg (struct intr_frame *f, unsigned argnum)
{
  /* Calculate the argument address */
  uint32_t *arg_point = (uint32_t *)f->esp;
  arg_point += (1 + argnum);

  /* Kill the process if address is false */
  if (!safemem_is_valid_buf (arg_point, 4))
    process_exit (-1);

  return *arg_point;
}