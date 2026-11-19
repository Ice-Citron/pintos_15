#ifndef VM_STACK_H
#define VM_STACK_H

#include <stdbool.h>
#include <stdint.h>
#include "threads/vaddr.h"

#define STACK_MAX (8 * 1024 * 1024)  /* 8 MB, the usual Linux default. */
#define STACK_BOTTOM ((uint8_t *) PHYS_BASE - STACK_MAX)
#define PUSHA_REACH 32               /* PUSHA faults 32 bytes below esp. */

bool stack_is_access (const void *addr, const void *esp);
bool stack_grow (void *addr);

#endif /* vm/stack.h */