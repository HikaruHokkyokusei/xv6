#include "./../kernel/types.h"
#include "./../user/vm.h"
#include "./../user/user.h"

static VM vmHolder[MAX_VM];

void initVMManager() {
  for (VM *vm = vmHolder; vm < &vmHolder[MAX_VM]; vm++) {
    u_lock_init(&vm->lock, "virtual machine lock");
    vm->pid = -1;
    vm->status = 0;
  }
}

uint8 killVM(VM *vm, int acc, int rel) {
  if (acc)
    u_lock_acquire(&vm->lock);

  uint8 res = 0;
  if (vm->status != 0) {
    vm_demote(vm->pid);
    kill(vm->pid);
    vm->pid = -1;
    vm->status = 0;
    res = 1;
  }

  if (rel)
    u_lock_release(&vm->lock);

  return res;
}

int createVM(char *workloadType) {
  int vmId;
  VM *vm;

  for (vm = vmHolder; vm < &vmHolder[MAX_VM]; vm++) {
    u_lock_acquire(&vm->lock);
    if (vm->status == 0) {
      goto FOUND;
    }
    u_lock_release(&vm->lock);
  }
  return -2; // Max VMs created.

  FOUND:
  vmId = fork();

  if (vmId == 0) {
    // Child VM
    VM *self = vm;
    while (self->status != 1) ;

    char *params[] = {workloadType};
    exec("/vmWorkload", params);

    killVM(self, 1, 1);
    exit(0);
  }

  if (vmId > 0) {
    // Parent Process. Only executed if Fork was successful.
    vm->pid = vmId;

    int res = vm_promote(vmId);
    if (res <= 0) {
      printf("VM Registration failed. Error code: %d.\n", res);
      killVM(vm, 0, 1);
      vmId = 0;
    } else {
      vm->status = 1;
    }
  }

  u_lock_release(&vm->lock);
  return vmId; // -1 If fork was unsuccessful else returns the id of the fork.
}

int deleteVM(int vmIdx) {
  if ((vmIdx < 0) || (vmIdx >= MAX_VM)) {
    return -1;
  }

  return killVM(&vmHolder[vmIdx], 1, 1);
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
