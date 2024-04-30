// Refer `hashmap.h` for implementation details.

typedef struct PAGE_HASHMAP_ENTRY_NODE {
    uint64 key; // Points to the physical address of a page.
    void *value;
    struct PAGE_HASHMAP_ENTRY_NODE *next;
} PAGE_HASHMAP_ENTRY_NODE;

typedef struct PAGE_HASHMAP {
    struct spinlock lock;
    PAGE_HASHMAP_ENTRY_NODE *entries[PAGE_HASHMAP_SIZE];
    int size;
} PAGE_HASHMAP;
