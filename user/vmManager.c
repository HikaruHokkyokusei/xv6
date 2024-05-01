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
    // If kill fails, then res will be set to -1.
    if ((res = kill(vm->pid)) == 0) {
      vm->pid = -1;
      vm->status = 0;
      res = 1;
    }
  }

  if (rel)
    u_lock_release(&vm->lock);

  return res;
}

VM *findAndLockVMSlot() {
  for (VM *vm = vmHolder; vm < &vmHolder[MAX_VM]; vm += 1) {
    u_lock_acquire(&vm->lock);
    if (vm->status == 0) {
      return vm;
    }
    u_lock_release(&vm->lock);
  }
  return 0x0;
}

void childProcess(char *workloadType) {
  // Note that after fork, the parent `vm` and child `vm`
  // have the same VA but DIFFERENT PA.
  char *params[] = {"vmWorkload", workloadType, 0};
  int res = exec("vmWorkload", params);
  printf("Workload exec failed: %d\n", res);
}

int parentProcess(VM *vm, int vmId, int sleepDuration) {
  if (sleepDuration > 0) { sleep(sleepDuration); }
  vm->pid = vmId;

  int res;
  if ((res = vm_promote(vmId)) <= 0) {
    printf("VM Registration failed. Error code: %d.\n", res);
    killVM(vm, 0, 0);
    return 0;
  } else {
    vm->status = 1;
  }

  return 1;
}

int createVM(char *workloadType) {
  VM *vm = findAndLockVMSlot();
  if (vm == 0x0) { return -2; } // Max VMs created.

  int vmId = fork();
  if (vmId == 0) {
    childProcess(workloadType);
    exit(0);
  } else if (vmId > 0) {
    if (parentProcess(vm, vmId, 5) == 0)
      vmId = 0;
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
