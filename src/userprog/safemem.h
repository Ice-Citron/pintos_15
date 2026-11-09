#ifndef USERPROG_SAFEMEM_H
#define USERPROG_SAFEMEM_H

#include "threads/interrupt.h"
#include "threads/thread.h"
#include "threads/vaddr.h"
#include "userprog/pagedir.h"

bool safemem_is_valid_addr (void *addr);
bool safemem_is_valid_buf (void *addr, unsigned size);
bool safemem_is_valid_string (char *str);
uint32_t safemem_read_syscall_arg (struct intr_frame *f, unsigned argnum);

#endif /* userprog/safemem.h */
