// Since this approach uses LL for resolving collisions,
// `HASH_MAP_SIZE` in `param.h` is preferably chosen to be
// a prime number to reduce the probability of collision.

typedef struct ENTRY_NODE {
    int key;
    void *value;
    struct ENTRY_NODE *next;
} ENTRY_NODE;

typedef struct HASHMAP {
    struct spinlock lock;
    // Creating `HASH_MAP_SIZE` number of Linked Lists to resolve collisions.
    ENTRY_NODE *entries[HASH_MAP_SIZE];
    int size;
} HASHMAP;
