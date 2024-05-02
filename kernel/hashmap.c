#include "types.h"
#include "param.h"
#include "riscv.h"
#include "spinlock.h"
#include "hashmap.h"
#include "defs.h"

uint hash(uint64 key) {
  return key % HASHMAP_SIZE;
}

void init_hashmap(HASHMAP *h) {
  initlock(&h->lock, "hashmap_lock");
  acquire(&h->lock);
  h->size = 0;
  // Initialize all `HASH_MAP_SIZE` number of `buckets` to NULL address.
  for (uint i = 0; i < HASHMAP_SIZE; i++) {
    h->entries[i] = 0x0;
  }
  release(&h->lock);
}

HASHMAP_ENTRY_NODE *get_hashmap_entry(HASHMAP *h, uint64 key) {
  push_off();
  int locked = holding(&h->lock);
  pop_off();
  if (locked == 0) {
    return 0x0;
  }
  HASHMAP_ENTRY_NODE *entry = h->entries[hash(key)];
  while (entry != 0x0) {
    if (entry->key == key) {
      return entry;
    }
    entry = entry->next;
  }
  return 0x0;
}

int hashmap_get(HASHMAP *h, uint64 key, void **value) {
  int ret = 0;
  acquire(&h->lock);
  HASHMAP_ENTRY_NODE *entry = get_hashmap_entry(h, key);
  if (entry != 0x0) {
    *value = entry->value;
    ret = 1;
  }
  release(&h->lock);
  return ret;
}

void hashmap_put(HASHMAP *h, uint64 key, void *value) {
  acquire(&h->lock);
  uint slot = hash(key);
  HASHMAP_ENTRY_NODE *entry = h->entries[slot];

  while (entry != 0x0) {
    if (entry->key == key) {
      goto FOUND;
    }
    entry = entry->next;
  }

  // Key does not exist, create a new HASHMAP_ENTRY_NODE
  if ((entry = ((HASHMAP_ENTRY_NODE *) kalloc())) == 0x0) {
    if (entry) // Unnecessary, but kept to prevent IDE warning.
      kfree(entry);
    panic("Unable to allocate memory...");
  }
  entry->next = h->entries[slot];
  h->entries[slot] = entry;
  entry->key = key;
  h->size += 1;

  FOUND:
  entry->value = value;
  release(&h->lock);
}

void hashmap_delete(HASHMAP *h, uint64 key) {
  acquire(&h->lock);
  uint slot = hash(key);
  HASHMAP_ENTRY_NODE *entry = h->entries[slot];
  HASHMAP_ENTRY_NODE *prev = entry;
  while (entry != 0x0) {
    if (entry->key == key) {
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

void hashmap_iterate(HASHMAP *h, void (*operate)(uint64, void *)) {
  acquire(&h->lock);
  HASHMAP_ENTRY_NODE *entry;
  for (uint i = 0; i < HASHMAP_SIZE; i++) {
    entry = h->entries[i];
    while (entry) {
      operate(entry->key, entry->value);
      entry = entry->next;
    }
  }
  release(&h->lock);
}

void hashmap_free(HASHMAP *h) {
  // BUG:
  // This `hashmap_free` implementation is not the best.
  // I have made a tradeoff between allowing segmentation
  // fault and allowing a deadlock. I am not freeing the
  // `HASHMAP` itself which will result in the lock becoming
  // invalid. But since the entries are freed, there can still be
  // errors. For the project, this is not the main focus.
  // Therefore, this is being ignored.
  acquire(&h->lock);
  for (uint i = 0; i < HASHMAP_SIZE; i++) {
    HASHMAP_ENTRY_NODE *entry = h->entries[i];
    while (entry) {
      HASHMAP_ENTRY_NODE *temp = entry;
      entry = entry->next;
      kfree(temp);
    }
  }
  release(&h->lock);
}
