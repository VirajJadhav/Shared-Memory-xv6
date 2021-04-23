#include "types.h"
#include "stat.h"
#include "user.h"
#include "mmu.h"
#include "ipc.h"
#include "shm.h"
#include "memlayout.h"

int main(int argc, char *argv[]) {
    int shmid = shmget(50, 2050, IPC_CREAT);
    if(shmid < 0) {
        printf(1, "shmget fail\n");
        exit();
    }

    char *ptr = (char *)shmat(shmid, (void *)0, 0);

    if(!ptr) {
        printf(1, "shmat fail\n");
        exit();
    }

    for(int i = 0; i < 5; i++) {
        ptr[i] = 'A';
    }

    printf(1, "Written: %s\n", ptr);

    int dt = shmdt(ptr);
    if(dt < 0) {
        printf(1, "shmdt fail\n");
        exit();
    }

    exit();
}