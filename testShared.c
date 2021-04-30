#include "types.h"
#include "stat.h"
#include "user.h"
#include "mmu.h"
#include "ipc.h"
#include "shm.h"
#include "memlayout.h"

#define KEY1 2000
#define KEY2 4000
#define KEY3 7777

int basicSharedTest();	// Create segment, write, read and destory test
void shmgetTest();	// variants of shmget
void shmctlTest();	// variants of shmctl
int forkTest();		// Two forks, parent write, child-1 write, child-2 write, parent read (parent attach)

int main(int argc, char *argv[]) {
    if(basicSharedTest() < 0) {
		printf(1, "failed\n");
	}
	shmgetTest();
	shmctlTest();
	if(forkTest() < 0) {
		printf(1, "failed\n");
	}

    exit();
}

int basicSharedTest() {
	printf(1, "* (Basic) Create segment, write, read and destory test : ");
	char *string = "Test String";
	int shmid = shmget(KEY1, 2565, 06 | IPC_CREAT);
	if(shmid < 0) {
		return -1;
	}
	char *ptr = (char *)shmat(shmid, (void *)0, 0);
	if((int)ptr < 0) {
		return -1;
	}
	for(int i = 0; string[i] != 0; i++) {
		ptr[i] = string[i];
	}

	int dt = shmdt(ptr);
	if(dt < 0) {
		return -1;
	}

	ptr = (char *)shmat(shmid, (void *)0, 0);
	if((int)ptr < 0) {
		return -1;
	}

	for(int i = 0; string[i] != 0; i++) {
		if(ptr[i] != string[i]) {
			return -1;
		}
	}

	dt = shmdt(ptr);
	if(dt < 0) {
		return -1;
	}
	int ctl = shmctl(shmid, IPC_RMID, (void *)0);
	if(ctl < 0) {
		return -1;
	}
	printf(1, "Pass\n");
	return 0;
}

void shmgetTest() {
	printf(1, "* Tests for variants of shmget :\n");
	printf(1, "\t- To check negative key input : ");
	int shmid = shmget(-1, 5000, 06 | IPC_CREAT);
	if(shmid < 0) {
		printf(1, "Pass\n");
	} else {
		printf(1, "Fail\n");
	}
	printf(1, "\t- Region permission other than Read / Read-Write : ");
	shmid = shmget(KEY1, 4000, IPC_CREAT);
	if(shmid < 0) {
		printf(1, "Pass\n");
	} else {
		printf(1, "Fail\n");
	}
	printf(1, "\t- Requesting region with more than decided pages ( > 64) : ");
	shmid = shmget(KEY1, 1.6e+7 + 40, 06 | IPC_CREAT);
	if(shmid < 0) {
		printf(1, "Pass\n");
	} else {
		printf(1, "Fail\n");
	}
	printf(1, "\t- Requesting region with zero size : ");
	shmid = shmget(KEY1, 0, 06 | IPC_CREAT);
	if(shmid < 0) {
		printf(1, "Pass\n");
	} else {
		printf(1, "Fail\n");
	}
	printf(1, "\t- Check for creation of valid region with IPC_CREAT : ");
	shmid = shmget(KEY1, 2000, 06 | IPC_CREAT);
	if(shmid < 0) {
		printf(1, "Fail\n");
	} else {
		printf(1, "Pass\n");
	}
	printf(1, "\t- Check for retrieving previously created region's shmid : ");
	int prevShmid = shmget(KEY1, 2000, 0);
	if(prevShmid == shmid) {
		printf(1, "Pass\n");
	} else {
		printf(1, "Fail\n");
	}
	printf(1, "\t- Check for creation of valid region with IPC_PRIVATE : ");
	shmid = shmget(IPC_PRIVATE, 2000, 06);
	if(shmid < 0) {
		printf(1, "Fail\n");
	} else {
		printf(1, "Pass\n");
	}
	printf(1, "\t- Check for IPC_CREAT | IPC_EXCL on existing region : ");
	shmid = shmget(KEY1, 0, 06 | IPC_CREAT | IPC_EXCL);
	if(shmid < 0) {
		printf(1, "Pass\n");
	} else {
		printf(1, "Fail\n");
	}
	printf(1, "\t- Check for IPC_EXCL alone, without IPC_CREAT : ");
	shmid = shmget(KEY2, 0, 06 | IPC_EXCL);
	if(shmid < 0) {
		printf(1, "Pass\n");
	} else {
		printf(1, "Fail\n");
	}
}

void shmctlTest() {
	printf(1, "* Tests for variants of shmctl :\n");
	char *string = "Test string";
	int shmid = shmget(KEY3, 8000, 06 | IPC_CREAT);
	if(shmid < 0) {
		printf(1, "\t- Fail shmctl tests\n");
		return;
	}
	char *ptr = (char *)shmat(shmid, (void *)0, 0);
	if((int)ptr < 0) {
		printf(1, "\t- Fail shmctl tests\n");
		return;
	}
	for(int i = 0; string[i] != 0; i++) {
		ptr[i] = string[i];
	}
	int dt = shmdt(ptr);
	if(dt < 0) {
		printf(1, "\t- Fail shmctl tests\n");
		return;
	}
	printf(1, "\t- Destroy / Remove (IPC_RMID) non - existing region : ");
	int ctl = shmctl(55555, IPC_RMID, (void *)0);
	if(ctl < 0) {
		printf(1, "Pass\n");
	} else {
		printf(1, "Fail\n");
	}

	struct shmid_ds bufferCheck;
	printf(1, "\t- Test IPC_STAT / SHM_STAT flags : ");
	ctl = shmctl(shmid, IPC_STAT, &bufferCheck);
	if(ctl < 0) {
		printf(1, "Fail\n");
	} else {
		printf(1, "Pass\n");
	}

	bufferCheck.shm_perm.mode = 04;
	printf(1, "\t- Test IPC_SET flag (change region mode to Read) : ");
	ctl = shmctl(shmid, IPC_SET, &bufferCheck);
	if(ctl < 0) {
		printf(1, "Fail\n");
	} else {
		printf(1, "Pass\n");
	}

	bufferCheck.shm_perm.mode = 567;
	printf(1, "\t- Test IPC_SET flag (change region mode to a random number) : ");
	ctl = shmctl(shmid, IPC_SET, &bufferCheck);
	if(ctl < 0) {
		printf(1, "Pass\n");
	} else {
		printf(1, "Fail\n");
	}

	printf(1, "\t- Destroy / Remove (IPC_RMID) existing region : ");
	ctl = shmctl(shmid, IPC_RMID, (void *)0);
	if(ctl < 0) {
		printf(1, "Fail\n");
	} else {
		printf(1, "Pass\n");
	}
}

int forkTest() {
	printf(1, "* 2 Forks (Parent attach; parent-child1-child2 write; parent read) : ");
	char *string = "AAAAABBBBBCCCCC";
	int shmid = shmget(KEY1, 2565, 06 | IPC_CREAT);
	if(shmid < 0) {
		return -1;
	}
	char *ptr = (char *)shmat(shmid, (void *)0, 0);
	if((int)ptr < 0) {
		return -1;
	}
	for(int i = 0; i < 5; i++) {
		ptr[i] = string[i];
	}
	int pid = fork();
	if(pid < 0) {
		return -1;
	} else if(pid == 0) {
		int pid1 = fork();
		if(pid1 < 0) {
			return -1;
		} else if(pid1 == 0) {
			for(int i = 5; i < 10; i++) {
				ptr[i] = string[i];
			}
		} else {
			wait();
			for(int i = 10; string[i] != 0; i++) {
				ptr[i] = string[i];
			}
		}
	} else {
		wait();
		for(int i = 0; string[i] != 0; i++) {
			if(ptr[i] != string[i]) {
				return -1;
			}
		}
		int dt = shmdt(ptr);
		if(dt < 0) {
			return -1;
		}
		int ctl = shmctl(shmid, IPC_RMID, (void *)0);
		if(ctl < 0) {
			return -1;
		}
		printf(1, "Pass\n");
		return 0;
	}
	return 0;
}
