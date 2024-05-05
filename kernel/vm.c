#include <stdarg.h>

#include "param.h"
#include "types.h"
#include "memlayout.h"
#include "elf.h"
#include "riscv.h"
#include "spinlock.h"
#include "hashmap.h"
#include "defs.h"
#include "fs.h"

// The kernel's page table.
pagetable_t kernel_pagetable;
HASHMAP cowPageRefCount;

extern char etext[];  // kernel.ld sets this to end of kernel code.

extern char trampoline[]; // trampoline.S

// Make a direct-map page table for the kernel.
pagetable_t
kvmmake(void) {
  pagetable_t kpgtbl;

  kpgtbl = (pagetable_t) kalloc();
  memset(kpgtbl, 0, PGSIZE);

  // uart registers
  kvmmap(kpgtbl, UART0, UART0, PGSIZE, PTE_R | PTE_W);

  // virtio mmio disk interface
  kvmmap(kpgtbl, VIRTIO0, VIRTIO0, PGSIZE, PTE_R | PTE_W);

  // PLIC
  kvmmap(kpgtbl, PLIC, PLIC, 0x400000, PTE_R | PTE_W);

  // map kernel text executable and read-only.
  kvmmap(kpgtbl, KERNBASE, KERNBASE, (uint64) etext - KERNBASE, PTE_R | PTE_X);

  // map kernel data and the physical RAM we'll make use of.
  kvmmap(kpgtbl, (uint64) etext, (uint64) etext, PHYSTOP - (uint64) etext, PTE_R | PTE_W);

  // map the trampoline for trap entry/exit to
  // the highest virtual address in the kernel.
  kvmmap(kpgtbl, TRAMPOLINE, (uint64) trampoline, PGSIZE, PTE_R | PTE_X);

  // allocate and map a kernel stack for each process.
  proc_mapstacks(kpgtbl);

  return kpgtbl;
}

// Initialize the one kernel_pagetable
void
kvminit(void) {
  init_hashmap(&cowPageRefCount);
  kernel_pagetable = kvmmake();
}

// Switch h/w page table register to the kernel's page table,
// and enable paging.
void
kvminithart() {
  // wait for any previous writes to the page table memory to finish.
  sfence_vma();

  w_satp(MAKE_SATP(kernel_pagetable));

  // flush stale entries from the TLB.
  sfence_vma();
}

// Create an empty user page table. Returns 0 if out of memory.
pagetable_t
uvmcreate() {
  pagetable_t pagetable;
  pagetable = (pagetable_t) kalloc();
  if (pagetable == 0)
    return 0;
  memset(pagetable, 0, PGSIZE);
  return pagetable;
}

// Recursively free page-table pages.
// All leaf mappings must already have been removed.
void
freewalk(pagetable_t pagetable) {
  // there are 2^9 = 512 PTEs in a page table.
  for (int i = 0; i < 512; i++) {
    pte_t pte = pagetable[i];
    if ((pte & PTE_V) && (pte & (PTE_R | PTE_W | PTE_X)) == 0) {
      // this PTE points to a lower-level page table.
      uint64 child = PTE2PA(pte);
      freewalk((pagetable_t) child);
      pagetable[i] = 0;
    } else if (pte & PTE_V) {
      panic("freewalk: leaf");
    }
  }
  kfree((void *) pagetable);
}

// Return the address of the PTE in page table pagetable
// that corresponds to virtual address va.  If alloc!=0,
// create any required page-table pages.
//
// The risc-v Sv39 scheme has three levels of page-table
// pages. A page-table page contains 512 64-bit PTEs.
// A 64-bit virtual address is split into five fields:
//   39..63 -- must be zero.
//   30..38 -- 9 bits of level-2 index.
//   21..29 -- 9 bits of level-1 index.
//   12..20 -- 9 bits of level-0 index.
//    0..11 -- 12 bits of byte offset within the page.
pte_t *
walk(pagetable_t pagetable, uint64 va, int alloc) {
  if (va >= MAXVA)
    panic("walk");

  for (int level = 2; level > 0; level--) {
    pte_t *pte = &pagetable[PX(level, va)];
    if (*pte & PTE_V) {
      pagetable = (pagetable_t) PTE2PA(*pte);
    } else {
      if (!alloc || (pagetable = (pde_t *) kalloc()) == 0) {
        if (alloc && pagetable != 0x0)
          kfree(pagetable);
        return 0;
      }
      memset(pagetable, 0, PGSIZE);
      *pte = PA2PTE(pagetable) | PTE_V;
    }
  }
  return &pagetable[PX(0, va)];
}

// Look up a virtual address, return the physical address, or 0 if not mapped.
// Can only be used to look up user pages.
uint64
walkaddr(pagetable_t pagetable, uint64 va) {
  pte_t *pte;
  uint64 pa;

  if (va >= MAXVA)
    return 0;

  pte = walk(pagetable, va, 0);
  if (pte == 0)
    return 0;
  if ((*pte & PTE_V) == 0)
    return 0;
  if ((*pte & PTE_U) == 0)
    return 0;
  pa = PTE2PA(*pte);
  return pa;
}

uint64
va2pa(pagetable_t pagetable, uint64 va) {
  uint64 baseAddr = walkaddr(pagetable, va);
  if (baseAddr == 0x0) return 0x0;
  else return baseAddr + (va - PGROUNDDOWN(va));
}

// Add a mapping to the kernel page table. Only used when booting. Does not flush TLB or enable paging.
void
kvmmap(pagetable_t kpgtbl, uint64 va, uint64 pa, uint64 sz, int perm) {
  if (mappages(kpgtbl, va, sz, pa, perm) != 0)
    panic("kvmmap");
}

void **refIncrement(uint8 exists, uint64 pa, void *oldCount, va_list params) {
  (void) (pa);
  (void) (params);

  void **res = (void **) kalloc();

  uint64 newCount = 0;
  if (exists) { newCount = (uint64) oldCount; }
  newCount += 1;

  res[0] = (void *) newCount;
  res[1] = (void *) ((uint64) 0);
  return res;
}

void **refDecrement(uint8 exists, uint64 pa, void *oldCount, va_list params) {
  if (exists == 0 || (uint64) oldCount == 0) { panic("Decrementing 0 ref count...\n"); }

  void **res = (void **) kalloc();

  uint64 newCount = ((uint64) oldCount) - 1;
  int do_free = va_arg(params, int);

  if (newCount == 0 && do_free == 1) {
    kfree((void *) pa);
  }

  res[0] = (void *) newCount;
  res[1] = (void *) ((uint64) (newCount == 0));
  return res;
}

/**
 * Create PTEs for virtual addresses starting at va that refer to
 * physical addresses starting at pa. va and size might not
 * be page-aligned. Returns 0 on success, -1 if walk() couldn't
 * allocate a needed page-table page.
 *
 * @param pagetable Reference to the Page Table of a process
 * @param va Virtual address
 * @param size Page Size, which is generally `PGSIZE` as defined in the `param.h`.
 * @param pa Physical Address that needs to be mapped to the VA
 * @param perm Permission that should be assigned to the PTE.
 * @return 0 is success else -1
 */
int
mappages(pagetable_t pagetable, uint64 va, uint64 size, uint64 pa, int perm) {
  uint64 currVA, lastVA;
  pte_t *pte;

  currVA = PGROUNDDOWN(va);
  lastVA = PGROUNDDOWN(va + size - 1);

  if (currVA > lastVA)
    panic("mappages: size");

  while (currVA <= lastVA) {
    if ((pte = walk(pagetable, currVA, 1)) == 0) { return -1; }
    if (*pte & PTE_V) { panic("mappages: remap"); }

    *pte = PA2PTE(pa) | perm | PTE_V;
    pa = PTE2PA(*pte);
    if ((pa != PRE_KERNEL_ADDRESS) && ((PTE_FLAGS(*pte) & PTE_COW) != 0)) {
      hashmap_update(&cowPageRefCount, pa, refIncrement);
    }

    currVA += PGSIZE;
    pa += PGSIZE;
  }
  return 0;
}

// Remove `npages` of mappings starting from va. va must be page-aligned.
// The mappings must exist. Optionally free the physical memory.
void
uvmunmap(pagetable_t pagetable, uint64 va, uint64 npages, int do_free) {
  if ((va % PGSIZE) != 0)
    panic("uvmunmap: not aligned");

  pte_t *pte;
  for (uint64 a = va; a < va + npages * PGSIZE; a += PGSIZE) {
    if ((pte = walk(pagetable, a, 0)) == 0)
      panic("uvmunmap: walk");
    if ((*pte & PTE_V) == 0)
      panic("uvmunmap: not mapped");
    if (PTE_FLAGS(*pte) == PTE_V)
      panic("uvmunmap: not a leaf");

    uint64 pa;
    if ((pa = PTE2PA(*pte)) != PRE_KERNEL_ADDRESS) {
      if ((PTE_FLAGS(*pte) & PTE_COW) != 0) {
        hashmap_update(&cowPageRefCount, pa, refDecrement, do_free);
      } else if (do_free) {
        kfree((void *) pa);
      }
    }
    *pte = 0;
  }
}

// Load the user initcode into address 0 of pagetable, for the very first process.
// sz must be less than a page.
void
uvmfirst(pagetable_t pagetable, uchar *src, uint sz) {
  char *mem;

  if (sz >= PGSIZE)
    panic("uvmfirst: more than a page");
  mem = kalloc();
  memset(mem, 0, PGSIZE);
  mappages(pagetable, 0, PGSIZE, (uint64) mem, PTE_W | PTE_R | PTE_X | PTE_U);
  memmove(mem, src, sz);
}

// Allocate PTEs and physical memory to grow process from oldsz to
// newsz, which need not be page aligned.  Returns new size or 0 on error.
uint64
uvmalloc(pagetable_t pagetable, uint64 oldsz, uint64 newsz, int xperm) {
  char *mem;
  uint64 a;

  if (newsz < oldsz)
    return oldsz;

  oldsz = PGROUNDUP(oldsz);
  for (a = oldsz; a < newsz; a += PGSIZE) {
    if ((mem = kalloc()) == 0x0) {
      uvmdealloc(pagetable, a, oldsz);
      return 0;
    }
    memset(mem, 0, PGSIZE);
    if (mappages(pagetable, a, PGSIZE, (uint64) mem, PTE_R | PTE_U | xperm) != 0) {
      kfree(mem);
      uvmdealloc(pagetable, a, oldsz);
      return 0;
    }
  }
  return newsz;
}

// Deallocate user pages to bring the process size from oldsz to
// newsz.  oldsz and newsz need not be page-aligned, nor does newsz
// need to be less than oldsz.  oldsz can be larger than the actual
// process size.  Returns the new process size.
uint64
uvmdealloc(pagetable_t pagetable, uint64 oldsz, uint64 newsz) {
  if (newsz >= oldsz)
    return oldsz;

  if (PGROUNDUP(newsz) < PGROUNDUP(oldsz)) {
    int npages = (PGROUNDUP(oldsz) - PGROUNDUP(newsz)) / PGSIZE;
    uvmunmap(pagetable, PGROUNDUP(newsz), npages, 1);
  }

  return newsz;
}

// Free user memory pages, then free page-table pages.
void
uvmfree(pagetable_t pagetable, uint64 sz) {
  if (sz > 0)
    uvmunmap(pagetable, 0, PGROUNDUP(sz) / PGSIZE, 1);
  freewalk(pagetable);
}

int
cowMapOrCopyPages(pagetable_t old, pagetable_t new, uint64 va, uint cow) {
  pte_t *pte;
  if ((pte = walk(old, va, 0)) == 0) { panic("cowMapOrCopyPages: pte should exist"); }
  if ((*pte & PTE_V) == 0) { panic("cowMapOrCopyPages: page not present"); }

  uint64 oldPA = PTE2PA(*pte);
  uint flags = PTE_FLAGS(*pte);

  uint64 newPA;
  if (cow == 0) {
    if ((newPA = (uint64) kalloc()) == 0x0) { return -1; }
    if (oldPA != PRE_KERNEL_ADDRESS) {
      memmove((char *) newPA, (char *) oldPA, PGSIZE);
    }
  } else if (cow == 1) {
    if (oldPA != PRE_KERNEL_ADDRESS) {
      if ((flags & PTE_COW) == 0) {
        if ((flags & PTE_W) != 0) { flags |= PTE_OLD_W; } else { flags &= (~PTE_OLD_W); }
      }
      flags &= (~PTE_W);
      flags |= PTE_COW;
    }
    newPA = oldPA;
  } else { return -1; }

  // If `cow == 0`, parent's PTE is unmapped and remapped w/o changes.
  // If `cow == 1`, parent's PTE is unmapped and remapped with new flags.
  uvmunmap(old, va, 1, 0);
  if ((mappages(old, va, PGSIZE, oldPA, flags) != 0) ||
      (mappages(new, va, PGSIZE, newPA, flags) != 0)) {
    if (cow == 0) { kfree((void *) newPA); }
    return -1;
  }
  return 0;
}

// Given a parent process's page table, copy its memory into a child's page table.
// Copies the page table. If `cow == 0`, then also copies the physical memory.
// Returns -1 on failure after freeing any allocated pages, else returns 0 on success.
int
uvmcopy(pagetable_t old, pagetable_t new, uint64 sz, uint cow) {
  uint64 currVA;

  int res;
  for (currVA = 0; currVA < sz; currVA += PGSIZE) {
    if ((res = cowMapOrCopyPages(old, new, currVA, cow)) < 0) {
      if (res == -1) { uvmunmap(new, 0, currVA / PGSIZE, 1); }
      return -1;
    }
  }
  return 0;
}

// mark a PTE invalid for user access.
// used by exec for the user stack guard page.
void
uvmclear(pagetable_t pagetable, uint64 va) {
  pte_t *pte;

  pte = walk(pagetable, va, 0);
  if (pte == 0)
    panic("uvmclear");
  *pte &= ~PTE_U;
}

// Copy from kernel to user.
// Copy len bytes from src to virtual address dstva in a given page table.
// Return 0 on success, -1 on error.
int
copyout(pagetable_t pagetable, uint64 dstva, char *src, uint64 len) {
  uint64 n, baseVA, basePA;
  pte_t *pte;

  while (len > 0) {
    baseVA = PGROUNDDOWN(dstva);
    basePA = walkaddr(pagetable, baseVA);
    if (basePA == 0) { return -1; }

    n = PGSIZE - (dstva - baseVA);
    if (n > len) { n = len; }

    pte = walk(pagetable, baseVA, 0);
    if ((*pte & PTE_COW) != 0) {
      handleCOWPageFault(pagetable, baseVA);
    } else if (basePA == PRE_KERNEL_ADDRESS) {
      handleDemandPageFault(pagetable, baseVA);
    }

    basePA = walkaddr(pagetable, baseVA);
    memmove((void *) (basePA + (dstva - baseVA)), src, n);

    len -= n;
    src += n;
    dstva = baseVA + PGSIZE;
  }
  return 0;
}

// Copy from user to kernel.
// Copy len bytes to dst from virtual address srcva in a given page table.
// Return 0 on success, -1 on error.
int
copyin(pagetable_t pagetable, char *dst, uint64 srcva, uint64 len) {
  uint64 n, baseVA, basePA;

  while (len > 0) {
    baseVA = PGROUNDDOWN(srcva);
    basePA = walkaddr(pagetable, baseVA);
    if (basePA == 0) { return -1; }

    n = PGSIZE - (srcva - baseVA);
    if (n > len) { n = len; }

    if (basePA == PRE_KERNEL_ADDRESS) {
      handleDemandPageFault(pagetable, baseVA);
      basePA = walkaddr(pagetable, baseVA);
    }
    memmove(dst, (void *) (basePA + (srcva - baseVA)), n);

    len -= n;
    dst += n;
    srcva = baseVA + PGSIZE;
  }
  return 0;
}

// Copy a null-terminated string from user to kernel.
// Copy bytes to dst from virtual address srcva in a given page table,
// until a '\0', or max.
// Return 0 on success, -1 on error.
int
copyinstr(pagetable_t pagetable, char *dst, uint64 srcva, uint64 max) {
  uint64 n, baseVA, basePA;
  int got_null = 0;

  while (got_null == 0 && max > 0) {
    baseVA = PGROUNDDOWN(srcva);
    basePA = walkaddr(pagetable, baseVA);
    if (basePA == 0) { return -1; }

    n = PGSIZE - (srcva - baseVA);
    if (n > max) { n = max; }

    if (basePA == PRE_KERNEL_ADDRESS) {
      handleDemandPageFault(pagetable, baseVA);
      basePA = walkaddr(pagetable, baseVA);
    }

    char *p = (char *) (basePA + (srcva - baseVA));
    while (n > 0) {
      if (*p == '\0') {
        *dst = '\0';
        got_null = 1;
        break;
      } else {
        *dst = *p;
      }
      --n;
      --max;
      p++;
      dst++;
    }

    srcva = baseVA + PGSIZE;
  }
  if (got_null) {
    return 0;
  } else {
    return -1;
  }
}

// See `int growproc(int);` in `proc.c`. Search `demand_alloc` to see
// how to enable the usage of this function.
uint64
demand_alloc(pagetable_t pagetable, uint64 oldsz, uint64 newsz) {
  if (newsz < oldsz)
    return oldsz;
  oldsz = PGROUNDUP(oldsz);

  for (uint64 currsz = oldsz; currsz < newsz; currsz += PGSIZE) {
    if (mappages(pagetable, currsz, PGSIZE, PRE_KERNEL_ADDRESS, PTE_U) != 0) {
      uvmdealloc(pagetable, currsz, oldsz);
      return 0;
    }
  }

  return newsz;
}

int handleDemandPageFault(pagetable_t pagetable, uint64 va) {
  if ((PTE_FLAGS(*walk(pagetable, va, 0)) & PTE_COW) != 0) {
    panic("Page is marked COW. Can't handle it for demand paging fault.");
  }

  if (walkaddr(pagetable, va) != PRE_KERNEL_ADDRESS) { return -1; }

  va = PGROUNDDOWN(va);
  uvmunmap(pagetable, va, 1, 0);

  char *demandedPage;
  if ((demandedPage = kalloc()) == 0x0) {
    printf("System out of RAM. Killing process...\n");
    return -1;
  } else if (mappages(pagetable, va, PGSIZE, (uint64) demandedPage, PTE_R | PTE_U | PTE_W) != 0) {
    kfree(demandedPage);
    panic("mappages() failed!");
  }
  return 0;
}

int
handleCOWPageFault(pagetable_t pagetable, uint64 va) {
  // BUG: Analyse why there is free list size drop when using COW forking.
  pte_t *pte = walk(pagetable, va, 0);
  uint64 flags = PTE_FLAGS(*pte);
  if ((flags & PTE_COW) == 0) { return -1; }

  uint64 pa = PTE2PA(*pte);
  uint64 newPage;
  if ((newPage = (uint64) kalloc()) == 0x0) { return -1; }
  if (pa != PRE_KERNEL_ADDRESS) { memmove((char *) newPage, (char *) pa, PGSIZE); }

  if ((flags & PTE_OLD_W) != 0) { flags |= PTE_W; }
  flags &= (~PTE_OLD_W);

  va = PGROUNDDOWN(va);
  uvmunmap(pagetable, va, 1, 0);
  if (mappages(pagetable, va, PGSIZE, newPage, (int) (flags & (~PTE_COW))) != 0) {
    kfree((void *) newPage);
    return -1;
  }

  return 0;
}
