#ifndef VM_PAGE_H
#define VM_PAGE_H

#include <hash.h>
#include <list.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include "filesys/off_t.h"
#include "threads/synch.h"

struct file;
struct frame;

/* Where a page's contents are when it is not in a frame. */
enum page_kind
  {
    PAGE_ZERO,    /* All zeroes (bss, stack) until first evicted dirty. */
    PAGE_FILE,    /* Executable segment: READ_BYTES from FILE, rest zero. */
    PAGE_MMAP,    /* Mapped file: written back to FILE, never swapped. */
    PAGE_SWAP     /* In swap slot SWAP_SLOT. */
  };

/* One user virtual page: an entry in a supplemental page table. */
struct page
  {
    void *upage;                  /* Page-aligned user address, the key. */
    uint32_t *pagedir;            /* Owner's page directory. */
    bool writable;                /* False for read-only code pages. */
    enum page_kind kind;          /* Where the data is when not in a frame. */
    struct file *file;            /* PAGE_FILE and PAGE_MMAP only. */
    off_t ofs;                    /* Offset of the page's data in FILE. */
    uint32_t read_bytes;          /* Bytes from FILE; the rest are zero. */
    size_t swap_slot;             /* PAGE_SWAP only. */
    struct frame *frame;          /* Frame holding it, or NULL. */
    struct lock lock;             /* Held while loading, saving, removing. */
    struct hash_elem elem;        /* In the owner's spt. */
    struct list_elem frame_elem;  /* In frame->pages. */
  };

bool page_table_init (struct hash *spt);
void page_table_destroy (struct hash *spt);
struct page *page_lookup (const void *uaddr);
bool page_add_file (void *upage, struct file *file, off_t ofs,
                    uint32_t read_bytes, bool writable);
bool page_add_zero (void *upage, bool writable);
bool page_add_mmap (void *upage, struct file *file, off_t ofs,
                    uint32_t read_bytes);
bool page_load (struct page *p, bool pin);
void page_unload (struct page *p);
void page_remove (struct page *p);
bool page_fault_handle (void *fault_addr, bool write, void *esp);
bool page_pin (const void *uaddr, bool write);
void page_unpin (const void *uaddr);

#endif /* vm/page.h */