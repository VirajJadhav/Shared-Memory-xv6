#include "types.h"
#include "stat.h"
#include "user.h"
#include "mmu.h"
#include "ipc.h"
#include "shm.h"
#include "memlayout.h"

int main(int argc, char *argv[]) {
    int shmid = shmget(50, 2000, IPC_CREAT);
    printf(1, "Used to test system calls. %d\n", shmid);
    char *str = (char*)shmat(shmid,(void*)0,0);
    for(int i = 0; i < 100; i++)
    {
        str[i] = 'a';
    }
    printf(1,"Stored data:\n");
    printf(1,"%s\n",str);

    int shmid1 = shmget(51, 16000, IPC_CREAT);
    printf(1, "Used to test system calls. %d\n", shmid1);
    char *str1 = (char*)shmat(shmid1,(void*)0,0);
    for(int i = 0; i < 100; i++)
    {
        str1[i] = 'b';
    }
    printf(1,"Stored data:\n");
    printf(1,"%s\n",str1);

    // int shmid2 = shmget(52, 2000, IPC_CREAT);
    // printf(1, "Used to test system calls. %d\n", shmid2);
    // char *str2 = (char*)shmat(shmid2,(void*)(HEAPLIMIT + 3*PGSIZE),SHM_REMAP);
    // for(int i = 0; i < 100; i++)
    // {
    //     str2[i] = 'c';
    // }
    // printf(1,"Stored data:\n");
    // printf(1,"%s\n",str2);
    
    shmdt((void*)str1);
    int shmid3 = shmget(53, 2000, IPC_CREAT);
    printf(1, "Used to test system calls. %d\n", shmid3);
    char *str3 = (char*)shmat(shmid3,(void*)(0),0);
    for(int i = 0; i < 100; i++)
    {
        str3[i] = 'd';
    }
    printf(1,"Stored data:\n");
    printf(1,"%s\n",str3);
    exit();
}