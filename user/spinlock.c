#include "./../kernel/types.h"
#include "./../kernel/spinlock.h"
#include "./../user/user.h"

void
u_lock_init(struct spinlock *lock, char *name)
{
  lock->locked = 0;
  lock->name = name;
}

void
u_lock_acquire(struct spinlock *lock)
{
  while(__sync_lock_test_and_set(&lock->locked, 1) != 0)
    ;
}

void
u_lock_release(struct spinlock * lock)
{
  __sync_lock_test_and_set(&lock->locked, 0)
    ;
}
