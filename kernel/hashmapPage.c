#include "types.h"
#include "param.h"
#include "riscv.h"
#include "spinlock.h"
#include "hashmap.h"
#include "defs.h"

#define ACCESS_TYPE uint64
#define GRANULARITY sizeof(ACCESS_TYPE)

// Refer `hashmap.c` for more implementation details.

uint hashPage(uint64 pa) {
  uint res = 0;
  for (uint64 address = pa; address < (pa + PGSIZE); address += GRANULARITY) {
    res += (*((ACCESS_TYPE *) address)) % PAGE_HASHMAP_SIZE; // This is crucial to prevent overflow.
    res %= PAGE_HASHMAP_SIZE;
  }
  return res;
}

uint8 pageEq(uint64 pa1, uint64 pa2) {
  if (PGROUNDDOWN(pa1) != pa1)
    panic("pa1 is not the base PA of the page.");
  if (PGROUNDDOWN(pa2) != pa2)
    panic("pa2 is not the base PA of the page.");

  if (pa1 == pa2)
    return 1;

  ACCESS_TYPE *baseAddr1 = (ACCESS_TYPE *) pa1, *baseAddr2 = (ACCESS_TYPE *) pa2;
  for (uint i = 0; i < PGSIZE; i += GRANULARITY) {
    if (*baseAddr1 != *baseAddr2)
      return 0;
    baseAddr1 += GRANULARITY;
    baseAddr2 += GRANULARITY;
  }
  return 1;
}

void init_pageHashmap(HASHMAP *h) {
  if ((PGSIZE % GRANULARITY) != 0)
    panic("PG SIZE is not multiple of GRANULARITY");
  initlock(&h->lock, "page_hashmap_lock");
  acquire(&h->lock);
  h->size = 0;
  for (uint i = 0; i < PAGE_HASHMAP_SIZE; i++) {
    h->entries[i] = 0x0;
  }
  release(&h->lock);
}

HASHMAP_ENTRY_NODE *get_page_hashmap_entry(HASHMAP *h, uint64 key) {
  push_off();
  int locked = holding(&h->lock);
  pop_off();
  if (locked == 0)
    return 0x0;
  HASHMAP_ENTRY_NODE *entry = h->entries[hashPage(key)];
  while (entry != 0x0) {
    if (pageEq(entry->key, key) == 1)
      return entry;
    entry = entry->next;
  }
  return 0x0;
}

int pageHashmap_get(HASHMAP *h, uint64 key, void **value) {
  if (PGROUNDDOWN(key) != key)
    panic("Key is not the base PA of the page.");
  int ret = 0;
  acquire(&h->lock);
  HASHMAP_ENTRY_NODE *entry = get_page_hashmap_entry(h, key);
  if (entry != 0x0) {
    *value = entry->value;
    ret = 1;
  }
  release(&h->lock);
  return ret;
}

void pageHashmap_put(HASHMAP *h, uint64 key, void *value) {
  acquire(&h->lock);
  uint slot = hashPage(key);
  HASHMAP_ENTRY_NODE *entry = h->entries[slot];

  while (entry != 0x0) {
    if (pageEq(entry->key, key) == 1) {
      goto FOUND;
    }
    entry = entry->next;
  }

  if ((entry = ((HASHMAP_ENTRY_NODE *) kalloc())) == 0x0) {
    if (entry)
      kfree(entry);
    panic("Unable to allocate memory...");
  }
  entry->next = h->entries[slot];
  h->entries[slot] = entry;
  h->size += 1;

  FOUND:
  entry->key = key;
  entry->value = value;
  release(&h->lock);
}

void pageHashmap_update(HASHMAP *h, uint64 key, void *(*update)(uint8, uint64, void *, va_list), ...) {
  acquire(&h->lock);
  uint slot = hashPage(key);
  HASHMAP_ENTRY_NODE *entry = h->entries[slot];
  HASHMAP_ENTRY_NODE *prev = entry;

  void **res;
  void *oldVal = 0x0;

  uint8 exists = 0;
  while (entry != 0x0) {
    if (pageEq(entry->key, key) == 1) {
      exists = 1;
      oldVal = entry->value;
      break;
    }
    prev = entry;
    entry = entry->next;
  }

  va_list params;
  va_start(params, update);
  res = update(exists, key, oldVal, params);
  va_end(params);

  if (exists == 1) {
    if (entry == 0x0) { panic("Hashmap Update: Found null enter\n"); }
    if (((uint64) res[1]) == 1) {
      if (prev == entry)
        h->entries[slot] = entry->next;
      prev->next = entry->next;
      h->size -= 1;
      kfree(entry);
    } else {
      entry->key = key;
      entry->value = res[0];
    }
  } else if (((uint64) res[1]) != 1) {
    // Key does not exist, create a new HASHMAP_ENTRY_NODE
    if ((entry = ((HASHMAP_ENTRY_NODE *) kalloc())) == 0x0) {
      if (entry) { kfree(entry); } // Unnecessary, but kept to prevent IDE warning.
      panic("Unable to allocate memory...");
    }
    entry->next = h->entries[slot];
    h->entries[slot] = entry;
    h->size += 1;

    entry->key = key;
    entry->value = res[0];
  }

  kfree(res);
  release(&h->lock);
}

void pageHashmap_delete(HASHMAP *h, uint64 key) {
  acquire(&h->lock);
  uint slot = hashPage(key);
  HASHMAP_ENTRY_NODE *entry = h->entries[slot];
  HASHMAP_ENTRY_NODE *prev = entry;
  while (entry != 0x0) {
    if (pageEq(entry->key, key) == 1) {
      if (prev == entry)
        h->entries[slot] = entry->next;
      prev->next = entry->next;
      kfree(entry);
      h->size -= 1;
      break;
    }
    prev = entry;
    entry = entry->next;
  }
  release(&h->lock);
}

void pageHashmap_iterate(HASHMAP *h, void (*operate)(uint64, void *)) {
  acquire(&h->lock);
  HASHMAP_ENTRY_NODE *entry;
  for (uint i = 0; i < PAGE_HASHMAP_SIZE; i++) {
    entry = h->entries[i];
    while (entry) {
      operate(entry->key, entry->value);
      entry = entry->next;
    }
  }
  release(&h->lock);
}

void pageHashmap_free(HASHMAP *h) {
  // BUG: Refer `hashmap.c`.
  acquire(&h->lock);
  for (uint i = 0; i < PAGE_HASHMAP_SIZE; i++) {
    HASHMAP_ENTRY_NODE *entry = h->entries[i];
    while (entry) {
      HASHMAP_ENTRY_NODE *temp = entry;
      entry = entry->next;
      kfree(temp);
    }
  }
  release(&h->lock);
}
