#include "./../kernel/types.h"
#include "./../user/user.h"
#include "./../kernel/riscv.h"

int countfree2() {
  // This code results in loss of Free Pages when using COW forking.
  // MAYBE: This is due to pages being allocated to Hashmap.
  if (fork() == 0) {
    printf("Child's Starting Size: 0x%x (%d)\n", (uint64) getsize(), PGROUNDDOWN(getsize()) / PGSIZE);
    int n = 0;
    while (1) {
      uint64 a = (uint64) sbrk(4096);
      if (a == -1) { break; }

      *(char *) (a + 4096 - 1) = 1; // Modify the memory to make sure it's really allocated.
      n += 1;
    }
    printf("Child's Closing  Size: 0x%x (%d)\n", (uint64) getsize(), PGROUNDDOWN(getsize()) / PGSIZE);
    exit(n);
  }

  int n = 0;
  wait(&n);

  return n;
}

int main(int argc, char *argv[]) {
  if (argc == 2 && strcmp(argv[1], "-t") == 0) {
    printf("Counting Process Started by Process: %d\n", getpid());
    int free0 = countfree2();
    int free1 = countfree2();

    if (free1 != free0) {
      printf("Lost Pages: %d -> %d\n", free0, free1);
    } else {
      printf("All good: %d\n", free0);
    }
  } else {
    int runtime = uptime();
    fprintf(1, "I have been running for %d.%d seconds...\n", runtime / 10, runtime % 10);
  }

  exit(0);
}
