#include "./../kernel/types.h"
#include "./../kernel/spinlock.h"
#include "./../user/user.h"

void
u_lock_init(struct spinlock *lock, char *name)
{
  lock->locked = 0;
  lock->name = name;
  lock->cpu = (struct cpu *) -1;
}

int
u_lock_holding(struct spinlock *lock)
{
  return (lock->locked && (((uint64) lock->cpu) == getpid()));
}

void
u_lock_acquire(struct spinlock *lock)
{
  if (u_lock_holding(lock) == 1) {
    printf("Lock already acquired.");
    exit(1);
  }

  while(__sync_lock_test_and_set(&lock->locked, 1) != 0);
  __sync_synchronize();

  lock->cpu = (struct cpu *) ((uint64) getpid());
}

void
u_lock_release(struct spinlock *lock)
{
  if (u_lock_holding(lock) != 1) {
    printf("Lock already released.");
    exit(1);
  }

  lock->cpu = (struct cpu *) -1;

  __sync_synchronize();
  __sync_lock_test_and_set(&lock->locked, 0);
}
