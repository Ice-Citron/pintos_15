#include "vm/page.h"
#include <debug.h>

/* Stubs so the VM build links.  Replaced by the real supplemental page
   table. */

bool
page_table_init (struct hash *spt UNUSED)
{
  return true;
}

void
page_table_destroy (struct hash *spt UNUSED)
{
}

struct page *
page_lookup (const void *uaddr UNUSED)
{
  return NULL;
}

bool
page_add_file (void *upage UNUSED, struct file *file UNUSED,
               off_t ofs UNUSED, uint32_t read_bytes UNUSED,
               bool writable UNUSED)
{
  return false;
}

bool
page_add_zero (void *upage UNUSED, bool writable UNUSED)
{
  return false;
}

bool
page_add_mmap (void *upage UNUSED, struct file *file UNUSED,
               off_t ofs UNUSED, uint32_t read_bytes UNUSED)
{
  return false;
}

bool
page_load (struct page *p UNUSED, bool pin UNUSED)
{
  return false;
}

void
page_unload (struct page *p UNUSED)
{
}

void
page_remove (struct page *p UNUSED)
{
}

bool
page_fault_handle (void *fault_addr UNUSED, bool write UNUSED,
                   void *esp UNUSED)
{
  return false;
}

bool
page_pin (const void *uaddr UNUSED, bool write UNUSED)
{
  return false;
}

void
page_unpin (const void *uaddr UNUSED)
{
}