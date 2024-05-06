typedef struct VMM_STATE {
    struct spinlock samplingTimeLock;
    uint lastSamplingTime;

    HASHMAP registeredVMs;
    HASHMAP knownPages;

    struct proc *lastSampledProcess;
    uint64 lastSampledVA;
} VMM_STATE;
