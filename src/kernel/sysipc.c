#include "common.h"

// int shmget(key_t key, size_t size, int shmflg);
uint64 sys_shmget(void) {
    return 0;
}

// int shmctl(int shmid, int cmd, struct shmid_ds *buf);
uint64 sys_shmctl(void) {
    return 0;
}

// void *shmat(int shmid, const void *shmaddr, int shmflg);
uint64 sys_shmat(void) {
    return 0;
}