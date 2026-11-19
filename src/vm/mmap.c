#include "vm/mmap.h"
#include <debug.h>

/* Stubs so the VM build links.  Replaced by memory mapped files. */

void
mmap_table_init (struct mmap_table *table UNUSED)
{
}

mapid_t
mmap_map (int fd UNUSED, void *addr UNUSED)
{
  return MAP_FAILED;
}

void
mmap_unmap (mapid_t id UNUSED)
{
}

void
mmap_unmap_all (void)
{
}

void
mmap_write_back (struct page *p UNUSED, const void *kpage UNUSED)
{
}