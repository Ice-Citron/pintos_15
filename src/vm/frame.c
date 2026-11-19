#include "vm/frame.h"
#include <debug.h>

/* Stubs so the VM build links.  Replaced by the real frame table. */

void
frame_init (void)
{
}

struct frame *
frame_alloc (struct page *p UNUSED)
{
  return NULL;
}

void
frame_release (struct frame *f UNUSED, struct page *p UNUSED)
{
}

struct frame *
frame_share_find (struct file *file UNUSED, off_t ofs UNUSED,
                  struct page *p UNUSED)
{
  return NULL;
}

void
frame_share_register (struct frame *f UNUSED, struct file *file UNUSED,
                      off_t ofs UNUSED)
{
}

void
frame_pin (struct frame *f UNUSED)
{
}

void
frame_unpin (struct frame *f UNUSED)
{
}