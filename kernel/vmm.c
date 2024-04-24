#include "types.h"
#include "param.h"
#include "riscv.h"
#include "spinlock.h"
#include "proc.h"
#include "hashmap.h"
#include "defs.h"

struct spinlock vmmLastSamplingTimeLock;
uint lastMemorySamplingTime = 0;

HASHMAP registeredVMs;

uint getCurrTime() {
  acquire(&tickslock);
  uint currTicks = ticks;
  release(&tickslock);
  return currTicks;
}

void vmmInit() {
  initlock(&vmmLastSamplingTimeLock, "VMM Mem sampling time lock");
  init_hashmap(&registeredVMs);
  acquire(&vmmLastSamplingTimeLock);
  lastMemorySamplingTime = getCurrTime();
  release(&vmmLastSamplingTimeLock);
}

void randomSampling() {
  acquire(&vmmLastSamplingTimeLock);
  uint currTime = getCurrTime();
  if (currTime - lastMemorySamplingTime >= VMMRANDTICKS) {
//    printf("%d -> %d <--> Performing random sampling...\n", lastMemorySamplingTime, currTime);
    lastMemorySamplingTime = currTime;
    // TODO: Code for random sampling
  }
  release(&vmmLastSamplingTimeLock);
}

int sys_vm_promote() {
  struct proc *p = myproc();
  int pid;
  argint(0, &pid);
  if (pid != p->pid) {
    return -1;
  }

  if (hashmap_get(&registeredVMs, pid, (void **) &p) == 1) {
    return 0;
  }
  hashmap_put(&registeredVMs, pid, p);
  return 1;
}
