#include "types.h"
#include "stat.h"
#include "user.h"
#include "mmu.h"
#include "ipc.h"
#include "shm.h"
#include "memlayout.h"

// test keys
#define KEY1 2000
#define KEY2 4000
#define KEY3 7777
#define KEY4 2006
#define KEY5 4001
#define KEY6 7778
#define KEY7 3567

#define allowedAddr HEAPLIMIT + 3*PGSIZE

int basicSharedTest();	// Create segment, write, read and destroy test
void shmgetTest();	// variants of shmget
void shmdtTest(); //variants of shmdt
void shmctlTest();	// variants of shmctl
void shmatTest(); // variants of shmat
void permissionTest();	// tests for permissions on shared memory region
int forkTest();		// Two forks, parent write, child-1 write, child-2 write, parent read (parent attach)

int main(int argc, char *argv[]) {
	/*
		Basic shared memory test,
		create region - write - read - remove region
	*/
    if(basicSharedTest() < 0) {
		printf(1, "failed\n");
	}
	// tests for different variants of shmget, w.r.t shmget flags
	shmgetTest();
	// tests for different variants of shmat, w.r.t shmat flags
	shmatTest();
	// tests for different variants of shmdt, w.r.t shmdt flags
	shmdtTest();
	// tests for different variants of shmctl, w.r.t shmctl flags
	shmctlTest();
	// tests for of different permissions on regions
	permissionTest();
	/* 
		test for fork,
		parent (attach) - parent write - child 1 write - child 2 write - parent read and verify - parent detach
	*/
	if(forkTest() < 0) {
		printf(1, "failed\n");
	}

    exit();
}

// basic shared test
int basicSharedTest() {
	printf(1, "* (Basic) Create segment, write, read and destroy test : ");
	char *string = "Test String";
	// get region
	int shmid = shmget(KEY1, 2565, 06 | IPC_CREAT);
	if(shmid < 0) {
		return -1;
	}
	// attach to shmid's region
	char *ptr = (char *)shmat(shmid, (void *)0, 0);
	if((int)ptr < 0) {
		return -1;
	}
	// write into region
	for(int i = 0; string[i] != 0; i++) {
		ptr[i] = string[i];
	}
	// detach
	int dt = shmdt(ptr);
	if(dt < 0) {
		return -1;
	}

	// re-attach for verification
	ptr = (char *)shmat(shmid, (void *)0, 0);
	if((int)ptr < 0) {
		return -1;
	}

	// read, written data
	for(int i = 0; string[i] != 0; i++) {
		if(ptr[i] != string[i]) {
			return -1;
		}
	}
	// detach
	dt = shmdt(ptr);
	if(dt < 0) {
		return -1;
	}
	// remove region
	int ctl = shmctl(shmid, IPC_RMID, (void *)0);
	if(ctl < 0) {
		return -1;
	}
	printf(1, "Pass\n");
	return 0;
}

// variants of shmget
void shmgetTest() {
	printf(1, "* Tests for variants of shmget :\n");
	printf(1, "\t- To check negative key input : ");
	// negative key
	int shmid = shmget(-1, 5000, 06 | IPC_CREAT);
	if(shmid < 0) {
		printf(1, "Pass\n");
	} else {
		printf(1, "Fail\n");
	}
	printf(1, "\t- Region permission other than Read / Read-Write (no permissions) : ");
	// invalid permission check
	shmid = shmget(KEY1, 4000, IPC_CREAT);
	if(shmid < 0) {
		printf(1, "Pass\n");
	} else {
		printf(1, "Fail\n");
	}
	printf(1, "\t- Requesting region with more than decided pages ( > 64) : ");
	// more than allowed size
	shmid = shmget(KEY1, 1.6e+7 + 40, 06 | IPC_CREAT);
	if(shmid < 0) {
		printf(1, "Pass\n");
	} else {
		printf(1, "Fail\n");
	}
	printf(1, "\t- Requesting region with zero size : ");
	// empty region
	shmid = shmget(KEY1, 0, 06 | IPC_CREAT);
	if(shmid < 0) {
		printf(1, "Pass\n");
	} else {
		printf(1, "Fail\n");
	}
	printf(1, "\t- Check for creation of valid region with IPC_CREAT : ");
	// IPC_CREAT
	shmid = shmget(KEY1, 2000, 06 | IPC_CREAT);
	if(shmid < 0) {
		printf(1, "Fail\n");
	} else {
		printf(1, "Pass\n");
	}
	printf(1, "\t- Check for retrieving previously created region's shmid : ");
	// get existing region shmid
	int prevShmid = shmget(KEY1, 2000, 0);
	if(prevShmid == shmid) {
		printf(1, "Pass\n");
	} else {
		printf(1, "Fail\n");
	}
	printf(1, "\t- Check for creation of valid region with IPC_PRIVATE : ");
	// IPC_PRIVATE
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
	// Only IPC_EXCL
	shmid = shmget(KEY2, 0, 06 | IPC_EXCL);
	if(shmid < 0) {
		printf(1, "Pass\n");
	} else {
		printf(1, "Fail\n");
	}
}

// variants of shmat
void shmatTest() {  
    int dt,i;
    char *ptr,*ptr2,*ptr3,*ptrarr[100];
	printf(1, "* Tests for variants of shmat :\n");
	int shmid = shmget(KEY4, 2565, 06 | IPC_CREAT);
    int shmid2 = shmget(KEY5,2565, 06 | IPC_CREAT);
    int shmid3 = shmget(KEY6,2565, 06 | IPC_CREAT);
	int shmid4 = shmget(KEY7,3000, 04 | IPC_CREAT);
	if(shmid < 0 || shmid2 < 0 || shmid3 < 0 || shmid4 < 0) {
		printf(1, "Fail\n");
        return;
	}
	printf(1, "\t- Non-existent shmid within allowed range: ");
	ptr = (char *)shmat(35, (void *)0, 0);
	if((int)ptr < 0) {
		printf(1,"Pass\n");
	} else {
		printf(1, "Fail\n");
	}
    printf(1, "\t- Check shmid beyond allowed range: ");
	ptr = (char *)shmat(1000, (void *)0, 0);
	if((int)ptr < 0) {
		printf(1,"Pass\n");
	} else {
		printf(1, "Fail\n");
	}
    printf(1, "\t- Requesting for address beyond lower limit: ");
	ptr = (char *)shmat(shmid, (void *)(HEAPLIMIT - 10), 0);
	if((int)ptr < 0) {
		printf(1,"Pass\n");
	} else {
		printf(1, "Fail\n");
	}
    printf(1, "\t- Requesting for address beyond upper limit: ");
	ptr = (char *)shmat(shmid, (void *)(KERNBASE + 10), 0);
	if((int)ptr < 0) {
		printf(1,"Pass\n");
	} else {
		printf(1, "Fail\n");
	}
    printf(1, "\t- Requesting for page-aligned address within range : ");
	ptr = (char*)shmat(shmid, (void *)(allowedAddr), 0);
	if((uint)ptr == allowedAddr) {
		printf(1,"Pass\n");
	} else {
        
		printf(1, "Fail\n");
	}
    printf(1, "\t- Corresponding detach : ");
    dt = shmdt(ptr);
    if(dt < 0) {
		printf(1, "Fail\n");
	} else {
        printf(1,"Pass\n");
    }
	printf(1, "\t- Requesting Read-Only access for Read-Only region : ");
	ptr = (char*)shmat(shmid4, (void *)(0), SHM_RDONLY);
	if((int)ptr != -1) {
		printf(1,"Allowed ! : Pass\n");
	} else {    
		printf(1, "Not allowed ! : Fail\n");
	}
    printf(1, "\t- Corresponding detach : ");
    dt = shmdt(ptr);
    if(dt < 0) {
		printf(1, "Fail\n");
	} else {
        printf(1,"Pass\n");
    }
    printf(1, "\t- Checking rounding down for non page-aligned address within range : ");
	// test attachment at rounded address after sending non page-aligned address with SHM_RND flag
	ptr = (char *)shmat(shmid, (void *)(allowedAddr + 7), SHM_RND);

	if((uint)ptr == allowedAddr) {
		printf(1,"Pass\n");
	} else {
		printf(1, "Fail\n");
	}
    printf(1, "\t- Corresponding detach : ");
    dt = shmdt(ptr);
    if(dt < 0) {
		printf(1, "Fail\n");
	} else {
        printf(1,"Pass\n");
    }
    printf(1, "\t- Checking compactness of memory mappings & filling of holes: ");
    ptr = (char *)shmat(shmid, (void *)0, 0);

	// attach at first available address
	if((uint)ptr != HEAPLIMIT) {
		printf(1, "Fail\n");
        printf(1,"%x",(uint)ptr);
        shmdt(ptr);
        goto nexttest;
	}
	// deliberately skip one address in between to check compactness of attachment 
    ptr2 = (char *)shmat(shmid2, (void *)(HEAPLIMIT + 2*PGSIZE), 0);
    if((uint)ptr2 != HEAPLIMIT+ 2*PGSIZE) {
		printf(1, "Fail\n");
        shmdt(ptr2);
        goto nexttest;
	}
    ptr3 = (char *)shmat(shmid3, (void *)(0), 0);

	// segment should be attached at the hole between the two regions previously attached
    if((uint)ptr3 == HEAPLIMIT + PGSIZE) {
		printf(1,"Pass\n");
	} else {
		printf(1, "Fail\n");
	}
    printf(1, "\t- Corresponding detaches (3) : \n ");
    dt = shmdt(ptr);
    if(dt < 0) {
		printf(1, "\t\t- Fail\n");
	}else {
        printf(1,"\t\t- Pass\n");
    }
    dt = shmdt(ptr2);
    if(dt < 0) {
		printf(1, "\t\t- Fail\n");
	} else {
        printf(1,"\t\t- Pass\n");
    }
    dt = shmdt(ptr3);
    if(dt < 0) {
		printf(1, "\t\t- Fail\n");
	} else {
        printf(1,"\t\t- Pass\n");
    }
	nexttest: printf(1, "\t- Trying to overwrite existing mapping without SHM_REMAP flag : ");
    ptr = (char *)shmat(shmid, (void *)HEAPLIMIT, 0);
	if((uint)ptr != HEAPLIMIT) {
		printf(1, "Fail\n");
        shmdt(ptr);
        goto nexttest2;
	}
    ptr2 = (char *)shmat(shmid2, (void *)(HEAPLIMIT + PGSIZE), 0);
    if((uint)ptr2 != HEAPLIMIT+ PGSIZE) {
		printf(1, "Fail\n");
        shmdt(ptr2);
        goto nexttest2;
	}
    ptr3 = (char *)shmat(shmid3, (void *)(HEAPLIMIT), 0);

	// -1 should be returned as remapping isnt allowed with explicit setting of SHM_REMAP flag
    if((int)ptr3 < 0) {
		printf(1,"Cannot Overwrite! : Pass\n");
	} else {
        shmdt(ptr3);
		printf(1, "Fail\n");
	}
    printf(1, "\t- Corresponding detaches (2) : \n ");
    dt = shmdt(ptr);
    if(dt < 0) {
		printf(1, "\t\t- Fail\n");
	} else {
        printf(1,"\t\t- Pass\n");
    }
    dt = shmdt(ptr2);
    if(dt < 0) {
		printf(1, "\t\t- Fail\n");
	} else {
        printf(1,"\t\t- Pass\n");
    }
	nexttest2: printf(1, "\t- Trying to overwrite existing mapping with SHM_REMAP flag: ");
    ptr = (char *)shmat(shmid, (void *)HEAPLIMIT, 0);
	if((uint)ptr != HEAPLIMIT) {
		printf(1, "Fail\n");
        shmdt(ptr);
        goto nexttest3;
	}
    ptr2 = (char *)shmat(shmid2, (void *)(HEAPLIMIT + PGSIZE), 0);
    if((uint)ptr2 != HEAPLIMIT+ PGSIZE) {
		printf(1, "Fail\n");
        shmdt(ptr);
        shmdt(ptr2);
        goto nexttest3;
	}
    ptr3 = (char *)shmat(shmid3, (void *)(HEAPLIMIT), SHM_REMAP);
    if((uint)ptr3 == HEAPLIMIT) {
		printf(1,"Can Overwrite! : Pass\n");
	} else {
		printf(1, "Fail\n");
	}
    printf(1, "\t- Corresponding detaches (3) : \n ");
    dt = shmdt(ptr);
    if(dt < 0) {
		printf(1, "\t\t- Fail\n");
	} else {
        printf(1,"\t\t- Pass\n");
    }
    dt = shmdt(ptr2);
    if(dt < 0) {
		printf(1, "\t\t- Fail\n");
	} else {
        printf(1,"\t\t- Pass\n");
    }
    dt = shmdt(ptr3);
	// ptr is already detached
    if(dt < 0) {
		printf(1, "\t\t- Pass\n");
	} else {
        printf(1,"\t\t- Fail\n");
    }
    nexttest3: printf(1, "\t- Trying to exhaust all regions for the process: ");
	for(i = 0; ; i++){
		ptrarr[i] = (char*)shmat(shmid,(void*)0,0);
		if((int)ptrarr[i] < 0){
			break;
		}
	}
	// should not allow a process to attach more than prescribed regions
	if(i == SHAREDREGIONS) {
		printf(1, "Pass\n");
	} else {
		printf(1,"Fail\n");
	}
	printf(1, "\t- Corresponding detaches (%d) : \n ",i);
	for(int j = 0; j < i; j++)
	{
		if(shmdt(ptrarr[j]) < 0) {
			printf(1,"Fail\n");
			goto ret;
		}
	}
	printf(1, "\t\t- All Passed\n");
	ret: return;
}

// variants of shmdt
void shmdtTest() {
	int dt,shmid;
	char* ptr;
	printf(1, "* Tests for variants of shmdt :\n");
	shmid = shmget(KEY4, 2565, 06 | IPC_CREAT);
	ptr = (char *)shmat(shmid, (void *)0, 0);
	printf(1, "\t- Trying to detach previously attached region: ");
	dt = shmdt(ptr);
    if(dt < 0) {
		printf(1, "Fail\n");
	} else {
		printf(1,"Pass\n");
	}
	printf(1, "\t- Trying to detach at unattached virtual address: ");
	// try detaching address which is not previously attached
	dt = shmdt((void*)(KERNBASE - PGSIZE));

    if(dt < 0) {
		printf(1, "Pass\n");
	} else {
		printf(1,"Fail\n");
	}
}	

// variants of shmctl
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
	// remove non - existing region
	int ctl = shmctl(55555, IPC_RMID, (void *)0);
	if(ctl < 0) {
		printf(1, "Pass\n");
	} else {
		printf(1, "Fail\n");
	}

	// user shmid_ds data structure
	struct shmid_ds bufferCheck;
	printf(1, "\t- Test IPC_STAT / SHM_STAT flags : ");
	ctl = shmctl(shmid, IPC_STAT, &bufferCheck);
	if(ctl < 0) {
		printf(1, "Fail\n");
	} else {
		printf(1, "Pass\n");
	}
	// change permission to read-only
	bufferCheck.shm_perm.mode = 04;
	printf(1, "\t- Test IPC_SET flag (change region mode to Read) : ");
	// set read-only permission to exisiting region
	ctl = shmctl(shmid, IPC_SET, &bufferCheck);
	if(ctl < 0) {
		printf(1, "Fail\n");
	} else {
		printf(1, "Pass\n");
	}

	bufferCheck.shm_perm.mode = 567;
	printf(1, "\t- Test IPC_SET flag (change region mode to a random number) : ");
	// try setting random permission
	ctl = shmctl(shmid, IPC_SET, &bufferCheck);
	if(ctl < 0) {
		printf(1, "Pass\n");
	} else {
		printf(1, "Fail\n");
	}

	printf(1, "\t- Destroy / Remove (IPC_RMID) existing region : ");
	// remove existing region
	ctl = shmctl(shmid, IPC_RMID, (void *)0);
	if(ctl < 0) {
		printf(1, "Fail\n");
	} else {
		printf(1, "Pass\n");
	}
}

// combinations of different permissions on region
void permissionTest() {
	printf(1, "* Tests for combinations of region permissions :\n");
	printf(1, "\t- Region permission other than Read / Read-Write : ");
	// invalid permission check
	int shmid = shmget(KEY1, 4000, 26 | IPC_CREAT);
	if(shmid < 0) {
		printf(1, "Pass\n");
	} else {
		printf(1, "Fail\n");
	}
	printf(1, "\t- Write into a read-write region : ");
	char testChar = 'a';
	shmid = shmget(KEY3, 2565, 06 | IPC_CREAT);
	if(shmid < 0) {
		printf(1, "Fail\n");
		return;
	}
	char *ptr = (char *)shmat(shmid, (void *)0, 0);
	if((int)ptr < 0) {
		printf(1, "Fail\n");
		return;
	}
	ptr[0] = testChar;
	int dt = shmdt(ptr);
	if(dt < 0) {
		printf(1, "Fail\n");
		return;
	}
	printf(1, "Pass\n");
	/* 
		change region mode to read-only	
	*/
	struct shmid_ds buffer;
	buffer.shm_perm.mode = 04;
	int ctl = shmctl(shmid, IPC_SET, &buffer);
	if(ctl < 0) {
		printf(1, "Fail\n");
		return;
	}
	printf(1, "\t- Read from a read-only region : ");
	ptr = (char *)shmat(shmid, (void *)0, 0);
	if((int)ptr < 0) {
		printf(1, "Fail\n");
		return;
	}
	if(ptr[0] != testChar) {
		printf(1, "Fail\n");
		return;
	}
	dt = shmdt(ptr);
	if(dt < 0) {
		printf(1, "Fail\n");
		return;
	}
	printf(1, "Pass\n");
	ctl = shmctl(shmid, IPC_RMID, (void *)0);
	if(ctl < 0) {
		printf(1, "Fail\n");
		return;
	}
	// printf(1, "\t- Write into a read-only region : ");
	// shmid = shmget(KEY7, 3000, 04 | IPC_CREAT);
	// if(shmid < 0) {
	// 	printf(1, "Fail\n");
	// 	return;
	// }
	// ptr = (char *)shmat(shmid, (void *)0, 0);
	// if((int)ptr < 0) {
	// 	printf(1, "Fail\n");
	// 	return;
	// }
	// ptr[0] = 'a';
	// dt = shmdt(ptr);
	// if(dt < 0) {
	// 	printf(1, "Fail\n");
	// 	return;
	// }
	// ctl = shmctl(shmid, IPC_RMID, (void *)0);
	// if(ctl < 0) {
	// 	printf(1, "Fail\n");
	// 	return;
	// }
}

// 2 fork test
int forkTest() {
	printf(1, "* 2 Forks (Parent attach; parent-child 1-child 2 write; parent read) : ");
	// test string
	char *string = "AAAAABBBBBCCCCC";
	// create
	int shmid = shmget(KEY1, 2565, 06 | IPC_CREAT);
	if(shmid < 0) {
		return -1;
	}
	// attach
	char *ptr = (char *)shmat(shmid, (void *)0, 0);
	if((int)ptr < 0) {
		return -1;
	}
	// parent-write
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
			// child 2-write
			for(int i = 5; i < 10; i++) {
				ptr[i] = string[i];
			}
		} else {
			wait();
			// child 1-write
			for(int i = 10; string[i] != 0; i++) {
				ptr[i] = string[i];
			}
		}
	} else {
		wait();
		// parent read-verify
		for(int i = 0; string[i] != 0; i++) {
			if(ptr[i] != string[i]) {
				return -1;
			}
		}
		// detach
		int dt = shmdt(ptr);
		if(dt < 0) {
			return -1;
		}
		// remove
		int ctl = shmctl(shmid, IPC_RMID, (void *)0);
		if(ctl < 0) {
			return -1;
		}
		printf(1, "Pass\n");
		return 0;
	}
	return 0;
}
