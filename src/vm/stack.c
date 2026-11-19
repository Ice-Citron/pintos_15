#include "vm/stack.h"
#include <debug.h>

/* Stubs so the VM build links.  Replaced by stack growth. */

bool
stack_is_access (const void *addr UNUSED, const void *esp UNUSED)
{
  return false;
}

bool
stack_grow (void *addr UNUSED)
{
  return false;
}