#include "types.h"
#include "param.h"
#include "riscv.h"
#include "defs.h"
#include "spinlock.h"

struct spinlock vmmLastSamplingTimeLock;
uint lastMemorySamplingTime = 0;

uint getCurrTime() {
  acquire(&tickslock);
  uint currTicks = ticks;
  release(&tickslock);
  return currTicks;
}

void vmminit() {
  initlock(&vmmLastSamplingTimeLock, "VMM Mem sampling time lock");
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
