#include "types.h"
#include "param.h"
#include "riscv.h"
#include "spinlock.h"
#include "hashmap.h"
#include "proc.h"
#include "hashmapPage.h"
#include "vmm.h"
#include "defs.h"

static VMM_STATE vmmState;

uint getCurrTime() {
  acquire(&tickslock);
  uint currTicks = ticks;
  release(&tickslock);
  return currTicks;
}

void vmmInit() {
  initlock(&vmmState.samplingTimeLock, "VMM Mem sampling time lock");
  init_hashmap(&vmmState.registeredVMs);
  init_pageHashmap(&vmmState.knownPages);
  acquire(&vmmState.samplingTimeLock);
  vmmState.lastSamplingTime = getCurrTime();
  release(&vmmState.samplingTimeLock);
}

void randomSampling() {
  acquire(&vmmState.samplingTimeLock);
  uint currTime = getCurrTime();
  uint64 timeDiff = currTime - vmmState.lastSamplingTime;
  if (((vmmState.lastSamplingTime == 0) && (timeDiff >= 100)) || (timeDiff >= VMMRANDTICKS)) {
//    printf("%d -> %d <--> Performing random sampling...\n", lastMemorySamplingTime, currTime);
    vmmState.lastSamplingTime = currTime;
    // TODO: Code for random sampling
  }
  release(&vmmState.samplingTimeLock);
}

void printRegisteredVM(uint64 key, void *value) {
  struct proc *p = (struct proc *) value;
  printf("PID: %d --> VM: (%d->%d, %p)\n", (int) key, p->parent->pid, p->pid, p);
}

void printRegisteredVMs() {
  printf("\nRegistered VMs:\n");
  hashmap_iterate(&vmmState.registeredVMs, printRegisteredVM);
  printf("\n");
}

int sys_vm_promote() {
  int childPid;
  argint(0, &childPid);
  struct proc *c; // Proc structure of the child to be promoted.
  if ((childPid == 0) || ((c = get_proc_from_pid(childPid)) == 0x0))
    return -2; // Invalid pid. No such process exists.

  struct proc *pp = myproc(); // Proc structure of the calling parent.
  if ((c->parent == 0x0) || (c->parent->pid != pp->pid))
    return -1; // Only a parent is allowed to request its child process to be promoted to a VM.

  if (hashmap_get(&vmmState.registeredVMs, childPid, (void **) &pp) == 1)
    return 0; // VM is already registered.

  hashmap_put(&vmmState.registeredVMs, childPid, c);

//  printRegisteredVMs();
  return 1;
}

int sys_vm_demote() {
  int childPid;
  argint(0, &childPid);
  struct proc *c; // Proc structure of the child to be demoted.
  if ((childPid == 0) || ((c = get_proc_from_pid(childPid)) == 0x0))
    return -2; // Invalid pid. No such process exists.

  if (c->pid == childPid)
    goto UNREGISTER;

  struct proc *pp = myproc(); // Proc structure of the calling parent.
  if ((c->parent == 0x0) || (c->parent->pid != pp->pid))
    return -1; // Only a parent or the child itself is allowed to request for the VM to be demoted.

  UNREGISTER:
  if (hashmap_get(&vmmState.registeredVMs, childPid, (void **) &pp) == 0)
    return 0; // VM is already unregistered.

  hashmap_delete(&vmmState.registeredVMs, childPid);

//  printRegisteredVMs();
  return 1;
}
