#include "./../kernel/types.h"
#include "./../kernel/spinlock.h"
#include "./../user/user.h"

#define MAX_VM 10

typedef struct VM {
    struct spinlock lock;

    int pid;
    int status; // 0 => Uninitialized or Killed; 1 => Active;
} VM;

static VM vmHolder[MAX_VM];

void initVMManager() {
  for (VM *vm = vmHolder; vm < &vmHolder[MAX_VM]; vm++) {
    u_lock_init(&vm->lock, "virtual machine lock");
    vm->pid = -1;
    vm->status = 0;
  }
}

int createVM() {
  int vmId;
  VM *vm;

  for (vm = vmHolder; vm < &vmHolder[MAX_VM]; vm++) {
    u_lock_acquire(&vm->lock);
    if (vm->status == 0) {
      goto FOUND;
    }
    u_lock_release(&vm->lock);
  }

  return -2;

  FOUND:
  vmId = fork();

  if (vmId == 0) {
    // Child VM
    VM *self = vm;

    vm_promote();

    // TODO: Execute its code... Maybe use `exec` syscall.
    while (1)
      ;

    u_lock_acquire(&self->lock);
    self->status = 0;
    u_lock_release(&self->lock);

    // TODO: Demote the VM...
    exit(0);
  }

  if (vmId > 0) {
    // Parent Process. Only executed if Fork was successful.
    vm->pid = vmId;
    vm->status = 1;
  }

  u_lock_release(&vm->lock);
  return vmId;
}

int deleteVM(int vmIdx) {
  if ((vmIdx < 0) || (vmIdx >= MAX_VM)) {
    return -1;
  }

  uint8 res;
  VM *vm = &vmHolder[vmIdx];

  u_lock_acquire(&vm->lock);
  if (vm->status == 0) {
    res = 0;
  } else {
    kill(vm->pid);
    vm->pid = -1;
    vm->status = 0;
    res = 1;
  }
  u_lock_release(&vm->lock);

  return res;
}

void printActiveVM() {
  uint idx = 0;

  printf("\nActive VM:\n");
  for (VM *vm = vmHolder; vm < &vmHolder[MAX_VM]; vm++) {
    if (vm->status == 1) {
      printf("\t(%d) -> Pid: %d\n", idx, vm->pid);
    }
    idx += 1;
  }
  printf("----------------------\n");
}
