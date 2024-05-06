#include "./../kernel/types.h"

struct stat;
struct spinlock;

// system calls
int fork(void);
int exit(int) __attribute__((noreturn));
int wait(int*);
int pipe(int*);
int write(int, const void*, int);
int read(int, void*, int);
int close(int);
int kill(int);
int exec(const char*, char**);
int open(const char*, int);
int mknod(const char*, short, short);
int unlink(const char*);
int fstat(int fd, struct stat*);
int link(const char*, const char*);
int mkdir(const char*);
int chdir(const char*);
int dup(int);
int getpid(void);
char* sbrk(int);
int sleep(int);
int uptime(void);
int getcpu(void);
int vm_promote(int);
int vm_demote(int);
int va2pa(uint64, uint8);
int getsize();

// user/ulib.c
char* strcpy(char*, const char*);
int strcmp(const char*, const char*);
uint strlen(const char*);
void* memset(void*, int, uint);
char* strchr(const char*, char c);
char* gets(char*, int max);
int stat(const char*, struct stat*);
int atoi(const char*);
void *memmove(void*, const void*, int);
int memcmp(const void *, const void *, uint);
void *memcpy(void *, const void *, uint);
char *strlower(char *);

// user/printf.c
void fprintf(int, const char*, ...);
void printf(const char*, ...);

// user/umalloc.c
void free(void *);
void* malloc(uint);

// user/spinlock.c
void u_lock_acquire(struct spinlock*);
void u_lock_init(struct spinlock*, char *);
void u_lock_release(struct spinlock*);

// user/vmManager.c
void initVMManager(void);
int createVM(char *);
int deleteVM(int);
void printActiveVM(void);
