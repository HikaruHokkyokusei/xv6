#include "types.h"
#include "param.h"
#include "riscv.h"
#include "spinlock.h"
#include "hashmap.h"
#include "defs.h"

uint hash(int key) {
  return key % HASH_MAP_SIZE;
}

void init_hashmap(HASHMAP *h) {
  initlock(&h->lock, "hashmap_lock");
  acquire(&h->lock);
  h->size = 0;
  // Initialize all `HASH_MAP_SIZE` number of `buckets` to NULL address.
  for (uint i = 0; i < HASH_MAP_SIZE; i++) {
    h->entries[i] = 0x0;
  }
  release(&h->lock);
}

ENTRY_NODE *get_hashmap_entry(HASHMAP *h, int key) {
  push_off();
  int locked = holding(&h->lock);
  pop_off();
  if (locked == 0) {
    return 0x0;
  }
  ENTRY_NODE *entry = h->entries[hash(key)];
  while (entry != 0x0) {
    if (entry->key == key) {
      return entry;
    }
    entry = entry->next;
  }
  return 0x0;
}

int hashmap_get(HASHMAP *h, int key, void **value) {
  int ret = 0;
  acquire(&h->lock);
  ENTRY_NODE *entry = get_hashmap_entry(h, key);
  if (entry != 0x0) {
    *value = entry->value;
    ret = 1;
  }
  release(&h->lock);
  return ret;
}

void hashmap_put(HASHMAP *h, int key, void *value) {
  acquire(&h->lock);
  uint slot = hash(key);
  ENTRY_NODE *entry = h->entries[slot];

  while (entry != 0x0) {
    if (entry->key == key) {
      goto FOUND;
    }
    entry = entry->next;
  }

  // Key does not exist, create a new ENTRY_NODE
  if ((entry = ((ENTRY_NODE *) kalloc())) == 0x0) {
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

void hashmap_delete(HASHMAP *h, int key) {
  acquire(&h->lock);
  uint slot = hash(key);
  ENTRY_NODE *entry = h->entries[slot];
  ENTRY_NODE *prev = entry;
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

void hashmap_iterate(HASHMAP *h, void (*operate)(int, void *)) {
  acquire(&h->lock);
  ENTRY_NODE *entry;
  for (uint i = 0; i < HASH_MAP_SIZE; i++) {
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
  for (uint i = 0; i < HASH_MAP_SIZE; i++) {
    ENTRY_NODE *entry = h->entries[i];
    while (entry) {
      ENTRY_NODE *temp = entry;
      entry = entry->next;
      kfree(temp);
    }
  }
  release(&h->lock);
}
