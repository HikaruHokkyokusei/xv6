// Since this approach uses LL for resolving collisions,
// `HASH_MAP_SIZE` in `param.h` is preferably chosen to be
// a prime number to reduce the probability of collision.

typedef struct HASHMAP_ENTRY_NODE {
    uint64 key;
    void *value;
    struct HASHMAP_ENTRY_NODE *next;
} HASHMAP_ENTRY_NODE;

typedef struct HASHMAP {
    struct spinlock lock;
    // Creating `HASH_MAP_SIZE` number of Linked Lists to resolve collisions.
    HASHMAP_ENTRY_NODE *entries[HASHMAP_SIZE];
    int size;
} HASHMAP;
