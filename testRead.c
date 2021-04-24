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

    printf(1, "Read: %s\n", ptr);

    

    int dt = shmdt(ptr);
    if(dt < 0) {
        printf(1, "shmdt fail\n");
        exit();
    }
    // char *ptr2 = (char *)shmat(shmid, (void *)0, 0);

    // if(!ptr2) {
    //     printf(1, "shmat fail\n");
    //     exit();
    // }

    // printf(1, "Read: %s\n", ptr2);
    // int dt2 = shmdt(ptr2);
    // if(dt2 < 0) {
    //     printf(1, "shmdt fail\n");
    //     exit();
    // }
    struct shmid_ds buffer;
    int ctl = shmctl(shmid, IPC_STAT, &buffer);
    // int ctl = shmctl(shmid, IPC_RMID, &buffer);
    if(ctl < 0) {
        printf(1, "shmctl fail\n");
        exit();
    }

    printf(1, "Data: \nSize: %d\nCount: %d\n", buffer.shm_segsz, buffer.shm_nattch);

    exit();
}