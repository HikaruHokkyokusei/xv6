#include "./../kernel/spinlock.h"

#define MAX_VM 10

typedef struct VM {
    struct spinlock lock;

    int pid;
    int status; // 0 => Uninitialized or Killed; 1 => Active;
} VM;
