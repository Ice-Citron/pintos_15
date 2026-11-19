#ifndef VM_MMAP_H
#define VM_MMAP_H

#include <list.h>
#include <stddef.h>

struct file;
struct page;

typedef int mapid_t;
#define MAP_FAILED ((mapid_t) -1)

/* One mmap mapping of a process. */
struct mmap
  {
    mapid_t id;                   /* Returned by mmap, unique per process. */
    struct file *file;            /* Own reference from file_reopen. */
    void *addr;                   /* First mapped page. */
    size_t page_cnt;              /* Number of pages mapped. */
    struct list_elem elem;        /* In the table's maps. */
  };

/* A process's mappings and the next id to give out.  Kept together so
   that only vm/mmap.c touches either. */
struct mmap_table
  {
    struct list maps;             /* struct mmap, oldest first. */
    mapid_t next_id;              /* Next mapping id to hand out. */
  };

void mmap_table_init (struct mmap_table *table);
mapid_t mmap_map (int fd, void *addr);
void mmap_unmap (mapid_t id);
void mmap_unmap_all (void);
void mmap_write_back (struct page *p, const void *kpage);

#endif /* vm/mmap.h */