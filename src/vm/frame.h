#ifndef VM_FRAME_H
#define VM_FRAME_H

#include <hash.h>
#include <list.h>
#include <stdbool.h>
#include "filesys/off_t.h"

struct file;
struct page;

/* A frame of user memory and the pages that map it. */
struct frame
  {
    void *kpage;                  /* Kernel address, from PAL_USER. */
    struct list pages;            /* Owners; more than one only if shared. */
    unsigned pin_cnt;             /* Never evicted while above 0. */
    bool evicting;                /* Being written out: do not share it. */
    struct file *share_file;      /* Shared read-only page's file, or NULL. */
    off_t share_ofs;              /* Its offset in that file. */
    struct hash_elem share_elem;  /* In the share table. */
    struct list_elem elem;        /* In the clock list. */
  };

void frame_init (void);
struct frame *frame_alloc (struct page *p);
void frame_release (struct frame *f, struct page *p);
struct frame *frame_share_find (struct file *file, off_t ofs,
                                struct page *p);
void frame_share_register (struct frame *f, struct file *file, off_t ofs);
void frame_pin (struct frame *f);
void frame_unpin (struct frame *f);

#endif /* vm/frame.h */