#ifndef USERPROG_SYSCALL_H
#define USERPROG_SYSCALL_H

#include "devices/shutdown.h"
#include "threads/interrupt.h"
#include "threads/thread.h"
#include "userprog/process.h"
#include "userprog/safemem.h"
#include "filesys/file.h"
#include "filesys/filesys.h"
#include <stdio.h>
#include <syscall-nr.h>

void syscall_init (void);

#endif /* userprog/syscall.h */