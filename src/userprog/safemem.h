#ifndef USERPROG_SAFEMEM_H
#define USERPROG_SAFEMEM_H

#include <stdbool.h>
#include <stdint.h>
#include "threads/interrupt.h"

bool safemem_is_valid_addr (const void *addr);
bool safemem_is_valid_buf (const void *addr, unsigned size);
bool safemem_is_writable_buf (void *addr, unsigned size);
bool safemem_is_valid_string (const char *str);
uint32_t safemem_read_syscall_arg (struct intr_frame *f, unsigned argnum);

#endif /* userprog/safemem.h */
