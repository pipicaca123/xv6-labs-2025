// Physical memory allocator, for user processes,
// kernel stacks, page-table pages,
// and pipe buffers. Allocates whole 4096-byte pages.

#include "types.h"
#include "param.h"
#include "memlayout.h"
#include "spinlock.h"
#include "riscv.h"
#include "defs.h"

#define ALLOCPAGESCNT 32768
struct kallocstat {
  uint8 phypages_refcnt[ALLOCPAGESCNT];
};
void freerange(void *pa_start, void *pa_end);
void kallocstatinit(void);

extern char end[]; // first address after kernel.
                   // defined by kernel.ld.
struct kallocstat alloc_stat_obj;

struct run {
  struct run *next;
};

struct {
  struct spinlock lock;
  struct run *freelist;
  struct spinlock cow_lock;
} kmem;

void
kinit()
{
  kallocstatinit();

  initlock(&kmem.lock, "kmem");
  initlock(&kmem.cow_lock, "kmem_cow");

  freerange(end, (void*)PHYSTOP);
}

void
freerange(void *pa_start, void *pa_end)
{
  char *p;
  p = (char*)PGROUNDUP((uint64)pa_start);
  for(; p + PGSIZE <= (char*)pa_end; p += PGSIZE)
    kfree(p);
}

// Free the page of physical memory pointed at by pa,
// which normally should have been returned by a
// call to kalloc().  (The exception is when
// initializing the allocator; see kinit above.)
void
kfree(void *pa)
{
  struct run *r;
  uint8 phyaddr_ref_cnt;

  if(((uint64)pa % PGSIZE) != 0 || (char*)pa < end || (uint64)pa >= PHYSTOP)
    panic("kfree");

  phyaddr_ref_cnt = kallocstatistics((uint64)pa, KALLOC_DEC);
  if (phyaddr_ref_cnt > 0) {
    // printf("reference more than 1, %d\n", phyaddr_ref_cnt);
    return;
  }
  // Fill with junk to catch dangling refs.
  memset(pa, 1, PGSIZE);

  r = (struct run*)pa;

  acquire(&kmem.lock);
  r->next = kmem.freelist;
  kmem.freelist = r;
  release(&kmem.lock);
}

// Allocate one 4096-byte page of physical memory.
// Returns a pointer that the kernel can use.
// Returns 0 if the memory cannot be allocated.
void *
kalloc(void)
{
  struct run *r;
  uint8 phyaddr_refcnt;

  acquire(&kmem.lock);
  r = kmem.freelist;
  if ((uint64)r >= PHYSTOP || (uint64)r < KERNBASE) {
    // TODO: maybe make freelist end points to NULL in initflow?
    printf("kalloc: no more free page!\n");
    release(&kmem.lock);
    return 0;
  }
  if(r)
    kmem.freelist = r->next;
  release(&kmem.lock);

  if(r)
    memset((char*)r, 5, PGSIZE); // fill with junk

  phyaddr_refcnt = kallocstatistics((uint64)r, KALLOC_INC);
  if (phyaddr_refcnt != 1) {
    printf("phyaddr_refcnt=%d\r\n", phyaddr_refcnt);
    panic("kalloc: phyaddr_refcnt should be 1!\n");
  }

  return (void *)r;
}

void kallocstatinit(void) {
  for (int i = 0; i < ALLOCPAGESCNT; i++) {
    // set 1 is due to kinit() will kfree() in freerange.
    alloc_stat_obj.phypages_refcnt[i] = 1;
  }
}
uint8 kallocstatistics(uint64 phyaddr, enum kallocstatoperate kalloc_stat_op) {
  int pageidx = ((phyaddr - KERNBASE) / 4096);
  uint8 *phypage_refcnt = &alloc_stat_obj.phypages_refcnt[pageidx];
  uint8 refcnt;

  acquire(&kmem.cow_lock);
  if ((kalloc_stat_op == KALLOC_DEC) && (*phypage_refcnt <= 0)) {
    panic("try to KALLOC_DEC a refcnt is 0's PA!\n");
  }
  switch (kalloc_stat_op) {
  case KALLOC_INC:
    (*phypage_refcnt)++;
    break;
  case KALLOC_DEC:
    (*phypage_refcnt)--;
    break;
  case KALLOC_GETCNT:
    // do nothing
    break;
  default:
    break;
  }
  refcnt = (*phypage_refcnt);
  release(&kmem.cow_lock);
  return refcnt;
}
