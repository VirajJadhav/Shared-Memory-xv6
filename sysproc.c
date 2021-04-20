#include "types.h"
#include "x86.h"
#include "defs.h"
#include "date.h"
#include "param.h"
#include "memlayout.h"
#include "mmu.h"
#include "proc.h"

int
sys_fork(void)
{
  return fork();
}

int
sys_exit(void)
{
  exit();
  return 0;  // not reached
}

int
sys_wait(void)
{
  return wait();
}

int
sys_kill(void)
{
  int pid;

  if(argint(0, &pid) < 0)
    return -1;
  return kill(pid);
}

int
sys_getpid(void)
{
  return myproc()->pid;
}

int
sys_sbrk(void)
{
  int addr;
  int n;

  if(argint(0, &n) < 0)
    return -1;
  addr = myproc()->sz;
  if(growproc(n) < 0)
    return -1;
  return addr;
}

int
sys_sleep(void)
{
  int n;
  uint ticks0;

  if(argint(0, &n) < 0)
    return -1;
  acquire(&tickslock);
  ticks0 = ticks;
  while(ticks - ticks0 < n){
    if(myproc()->killed){
      release(&tickslock);
      return -1;
    }
    sleep(&ticks, &tickslock);
  }
  release(&tickslock);
  return 0;
}

// return how many clock tick interrupts have occurred
// since start.
int
sys_uptime(void)
{
  uint xticks;

  acquire(&tickslock);
  xticks = ticks;
  release(&tickslock);
  return xticks;
}

// Shared memory

extern int shmget(uint, uint, int);

int
sys_shmget(void)
{
  int key, size, shmflag;
  if(argint(0, &key) < 0)
    return -1;
  if(argint(1, &size) < 0)
    return -1;
  if(argint(2, &shmflag) < 0)
    return -1;
  return shmget((uint)key, (uint)size, shmflag);
}

extern int shmdt(void*);

int sys_shmdt(void)
{
  int i;
  if(argint(0,&i)<0)
    return 0;
  return shmdt((void*)i);
}

extern void * shmat(int, void*, int);


void*
sys_shmat(void)
{
  int shmid, shmflag;
  int i;
  if(argint(0, &shmid) < 0)
    return (void*)0;
  if(argint(1,&i)<0)
    return (void*)0;
  if(argint(2, &shmflag) < 0)
    return (void*)0;
  return shmat(shmid, (void*)i, shmflag);
}

extern int shmctl(int, int, void*);

int
sys_shmctl(void)
{
  int shmid, cmd, buf;
  if(argint(0, &shmid) < 0)
    return -1;
  if(argint(1, &cmd) < 0)
    return -1;
  if(argint(2, &buf) < 0)
    return -1;
  return shmctl(shmid, cmd, (void*)buf);
}