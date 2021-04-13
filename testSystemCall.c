#include "types.h"
#include "stat.h"
#include "user.h"

#include "ipc.h"

int main(int argc, char *argv[]) {
    int shmid = shmget(50, 2000, IPC_CREAT);
    printf(1, "Used to test system calls. %d\n", shmid);
    // char *str = (char*)shmat(shmid,(void*)0,0);
    // gets(str,20);
    exit();
}