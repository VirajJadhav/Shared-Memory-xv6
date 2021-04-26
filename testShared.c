#include "types.h"
#include "stat.h"
#include "user.h"
#include "mmu.h"
#include "ipc.h"
#include "shm.h"
#include "memlayout.h"

int main(int argc, char *argv[]) {
    int shmid = shmget(2000, 2050, 06 | IPC_CREAT);
	if(shmid < 0) {
		printf(1, "fail shmget\n");
        exit();
	}
	char *ptr1 = (char *)shmat(shmid, (void *)0,0);
	if(!ptr1) {
		printf(1, "Fail shmat\n");
        exit();
	}
	int pid = fork();
	if(pid == 0) {
		for(int i = 0; i < 5; i++) {
			ptr1[i] = 'A';
		}
	} else {
		wait();
		printf(1, "Read: \n");
		for(int i = 0; i < 5; i++) {
			printf(1, "%c", ptr1[i]);
		}
		int dt = shmdt(ptr1);
		if(dt < 0) {
			printf(1, "Fail shmdt\n");
            exit();
		}

		struct shmid_ds check;

		int ct = shmctl(shmid, IPC_STAT, &check);
		if(ct < 0) {
			printf(1, "Fail shmctl\n");
            exit();
		}

		printf(1, "\nData: \n%d\n%d\n%d\n", check.shm_perm.__key, check.shm_segsz, check.shm_nattch);
	}

    exit();
}