#include "param.h"
#include "types.h"
#include "defs.h"
#include "x86.h"
#include "memlayout.h"
#include "mmu.h"
#include "proc.h"
#include "elf.h"

#include "shm.h"
#include "ipc.h"

extern char data[];  // defined by kernel.ld
pde_t *kpgdir;  // for use in scheduler()

// Set up CPU's kernel segment descriptors.
// Run once on entry on each CPU.
void
seginit(void)
{
  struct cpu *c;

  // Map "logical" addresses to virtual addresses using identity map.
  // Cannot share a CODE descriptor for both kernel and user
  // because it would have to have DPL_USR, but the CPU forbids
  // an interrupt from CPL=0 to DPL=3.
  c = &cpus[cpuid()];
  c->gdt[SEG_KCODE] = SEG(STA_X|STA_R, 0, 0xffffffff, 0);
  c->gdt[SEG_KDATA] = SEG(STA_W, 0, 0xffffffff, 0);
  c->gdt[SEG_UCODE] = SEG(STA_X|STA_R, 0, 0xffffffff, DPL_USER);
  c->gdt[SEG_UDATA] = SEG(STA_W, 0, 0xffffffff, DPL_USER);
  lgdt(c->gdt, sizeof(c->gdt));
}

// Return the address of the PTE in page table pgdir
// that corresponds to virtual address va.  If alloc!=0,
// create any required page table pages.
static pte_t *
walkpgdir(pde_t *pgdir, const void *va, int alloc)
{
  pde_t *pde;
  pte_t *pgtab;

  pde = &pgdir[PDX(va)];
  if(*pde & PTE_P){
    pgtab = (pte_t*)P2V(PTE_ADDR(*pde));
  } else {
    if(!alloc || (pgtab = (pte_t*)kalloc()) == 0)
      return 0;
    // Make sure all those PTE_P bits are zero.
    memset(pgtab, 0, PGSIZE);
    // The permissions here are overly generous, but they can
    // be further restricted by the permissions in the page table
    // entries, if necessary.
    *pde = V2P(pgtab) | PTE_P | PTE_W | PTE_U;
  }
  return &pgtab[PTX(va)];
}

// Create PTEs for virtual addresses starting at va that refer to
// physical addresses starting at pa. va and size might not
// be page-aligned.
static int
mappages(pde_t *pgdir, void *va, uint size, uint pa, int perm)
{
  char *a, *last;
  pte_t *pte;

  a = (char*)PGROUNDDOWN((uint)va);
  last = (char*)PGROUNDDOWN(((uint)va) + size - 1);
  for(;;){
    if((pte = walkpgdir(pgdir, a, 1)) == 0)
      return -1;
    if(*pte & PTE_P)
      panic("remap");
    *pte = pa | perm | PTE_P;
    if(a == last)
      break;
    a += PGSIZE;
    pa += PGSIZE;
  }
  return 0;
}

// There is one page table per process, plus one that's used when
// a CPU is not running any process (kpgdir). The kernel uses the
// current process's page table during system calls and interrupts;
// page protection bits prevent user code from using the kernel's
// mappings.
//
// setupkvm() and exec() set up every page table like this:
//
//   0..KERNBASE: user memory (text+data+stack+heap), mapped to
//                phys memory allocated by the kernel
//   KERNBASE..KERNBASE+EXTMEM: mapped to 0..EXTMEM (for I/O space)
//   KERNBASE+EXTMEM..data: mapped to EXTMEM..V2P(data)
//                for the kernel's instructions and r/o data
//   data..KERNBASE+PHYSTOP: mapped to V2P(data)..PHYSTOP,
//                                  rw data + free physical memory
//   0xfe000000..0: mapped direct (devices such as ioapic)
//
// The kernel allocates physical memory for its heap and for user memory
// between V2P(end) and the end of physical memory (PHYSTOP)
// (directly addressable from end..P2V(PHYSTOP)).

// This table defines the kernel's mappings, which are present in
// every process's page table.
static struct kmap {
  void *virt;
  uint phys_start;
  uint phys_end;
  int perm;
} kmap[] = {
 { (void*)KERNBASE, 0,             EXTMEM,    PTE_W}, // I/O space
 { (void*)KERNLINK, V2P(KERNLINK), V2P(data), 0},     // kern text+rodata
 { (void*)data,     V2P(data),     PHYSTOP,   PTE_W}, // kern data+memory
 { (void*)DEVSPACE, DEVSPACE,      0,         PTE_W}, // more devices
};

// Set up kernel part of a page table.
pde_t*
setupkvm(void)
{
  pde_t *pgdir;
  struct kmap *k;

  if((pgdir = (pde_t*)kalloc()) == 0)
    return 0;
  memset(pgdir, 0, PGSIZE);
  if (P2V(PHYSTOP) > (void*)DEVSPACE)
    panic("PHYSTOP too high");
  for(k = kmap; k < &kmap[NELEM(kmap)]; k++)
    if(mappages(pgdir, k->virt, k->phys_end - k->phys_start,
                (uint)k->phys_start, k->perm) < 0) {
      freevm(pgdir);
      return 0;
    }
  return pgdir;
}

// Allocate one page table for the machine for the kernel address
// space for scheduler processes.
void
kvmalloc(void)
{
  kpgdir = setupkvm();
  switchkvm();
}

// Switch h/w page table register to the kernel-only page table,
// for when no process is running.
void
switchkvm(void)
{
  lcr3(V2P(kpgdir));   // switch to the kernel page table
}

// Switch TSS and h/w page table to correspond to process p.
void
switchuvm(struct proc *p)
{
  if(p == 0)
    panic("switchuvm: no process");
  if(p->kstack == 0)
    panic("switchuvm: no kstack");
  if(p->pgdir == 0)
    panic("switchuvm: no pgdir");

  pushcli();
  mycpu()->gdt[SEG_TSS] = SEG16(STS_T32A, &mycpu()->ts,
                                sizeof(mycpu()->ts)-1, 0);
  mycpu()->gdt[SEG_TSS].s = 0;
  mycpu()->ts.ss0 = SEG_KDATA << 3;
  mycpu()->ts.esp0 = (uint)p->kstack + KSTACKSIZE;
  // setting IOPL=0 in eflags *and* iomb beyond the tss segment limit
  // forbids I/O instructions (e.g., inb and outb) from user space
  mycpu()->ts.iomb = (ushort) 0xFFFF;
  ltr(SEG_TSS << 3);
  lcr3(V2P(p->pgdir));  // switch to process's address space
  popcli();
}

// Load the initcode into address 0 of pgdir.
// sz must be less than a page.
void
inituvm(pde_t *pgdir, char *init, uint sz)
{
  char *mem;

  if(sz >= PGSIZE)
    panic("inituvm: more than a page");
  mem = kalloc();
  memset(mem, 0, PGSIZE);
  mappages(pgdir, 0, PGSIZE, V2P(mem), PTE_W|PTE_U);
  memmove(mem, init, sz);
}

// Load a program segment into pgdir.  addr must be page-aligned
// and the pages from addr to addr+sz must already be mapped.
int
loaduvm(pde_t *pgdir, char *addr, struct inode *ip, uint offset, uint sz)
{
  uint i, pa, n;
  pte_t *pte;

  if((uint) addr % PGSIZE != 0)
    panic("loaduvm: addr must be page aligned");
  for(i = 0; i < sz; i += PGSIZE){
    if((pte = walkpgdir(pgdir, addr+i, 0)) == 0)
      panic("loaduvm: address should exist");
    pa = PTE_ADDR(*pte);
    if(sz - i < PGSIZE)
      n = sz - i;
    else
      n = PGSIZE;
    if(readi(ip, P2V(pa), offset+i, n) != n)
      return -1;
  }
  return 0;
}

// Allocate page tables and physical memory to grow process from oldsz to
// newsz, which need not be page aligned.  Returns new size or 0 on error.
int
allocuvm(pde_t *pgdir, uint oldsz, uint newsz)
{
  char *mem;
  uint a;

  if(newsz >= HEAPLIMIT) // prev value: KERBASE
    return 0;
  if(newsz < oldsz)
    return oldsz;

  a = PGROUNDUP(oldsz);
  for(; a < newsz; a += PGSIZE){
    mem = kalloc();
    if(mem == 0){
      cprintf("allocuvm out of memory\n");
      deallocuvm(pgdir, newsz, oldsz);
      return 0;
    }
    memset(mem, 0, PGSIZE);
    if(mappages(pgdir, (char*)a, PGSIZE, V2P(mem), PTE_W|PTE_U) < 0){
      cprintf("allocuvm out of memory (2)\n");
      deallocuvm(pgdir, newsz, oldsz);
      kfree(mem);
      return 0;
    }
  }
  return newsz;
}

// Deallocate user pages to bring the process size from oldsz to
// newsz.  oldsz and newsz need not be page-aligned, nor does newsz
// need to be less than oldsz.  oldsz can be larger than the actual
// process size.  Returns the new process size.
int
deallocuvm(pde_t *pgdir, uint oldsz, uint newsz)
{
  pte_t *pte;
  uint a, pa;

  if(newsz >= oldsz)
    return oldsz;

  a = PGROUNDUP(newsz);
  for(; a  < oldsz; a += PGSIZE){
    pte = walkpgdir(pgdir, (char*)a, 0);
    if(!pte)
      a = PGADDR(PDX(a) + 1, 0, 0) - PGSIZE;
    else if((*pte & PTE_P) != 0){
      pa = PTE_ADDR(*pte);
      if(pa == 0)
        panic("kfree");
      char *v = P2V(pa);
      kfree(v);
      *pte = 0;
    }
  }
  return newsz;
}

// Free a page table and all the physical memory pages
// in the user part.
void
freevm(pde_t *pgdir)
{
  uint i;

  if(pgdir == 0)
    panic("freevm: no pgdir");
  // deallocuvm(pgdir, KERNBASE, 0);
  deallocuvm(pgdir, HEAPLIMIT, 0);
  for(i = 0; i < NPDENTRIES; i++){
    if(pgdir[i] & PTE_P){
      char * v = P2V(PTE_ADDR(pgdir[i]));
      kfree(v);
    }
  }
  kfree((char*)pgdir);
}

// Clear PTE_U on a page. Used to create an inaccessible
// page beneath the user stack.
void
clearpteu(pde_t *pgdir, char *uva)
{
  pte_t *pte;

  pte = walkpgdir(pgdir, uva, 0);
  if(pte == 0)
    panic("clearpteu");
  *pte &= ~PTE_U;
}

// Given a parent process's page table, create a copy
// of it for a child.
pde_t*
copyuvm(pde_t *pgdir, uint sz)
{
  pde_t *d;
  pte_t *pte;
  uint pa, i, flags;
  char *mem;

  if((d = setupkvm()) == 0)
    return 0;
  for(i = 0; i < sz; i += PGSIZE){
    if((pte = walkpgdir(pgdir, (void *) i, 0)) == 0)
      panic("copyuvm: pte should exist");
    if(!(*pte & PTE_P))
      panic("copyuvm: page not present");
    pa = PTE_ADDR(*pte);
    flags = PTE_FLAGS(*pte);
    if((mem = kalloc()) == 0)
      goto bad;
    memmove(mem, (char*)P2V(pa), PGSIZE);
    if(mappages(d, (void*)i, PGSIZE, V2P(mem), flags) < 0) {
      kfree(mem);
      goto bad;
    }
  }
  return d;

bad:
  freevm(d);
  return 0;
}

//PAGEBREAK!
// Map user virtual address to kernel address.
char*
uva2ka(pde_t *pgdir, char *uva)
{
  pte_t *pte;

  pte = walkpgdir(pgdir, uva, 0);
  if((*pte & PTE_P) == 0)
    return 0;
  if((*pte & PTE_U) == 0)
    return 0;
  return (char*)P2V(PTE_ADDR(*pte));
}

// Copy len bytes from p to user address va in page table pgdir.
// Most useful when pgdir is not the current page table.
// uva2ka ensures this only works for PTE_U pages.
int
copyout(pde_t *pgdir, uint va, void *p, uint len)
{
  char *buf, *pa0;
  uint n, va0;

  buf = (char*)p;
  while(len > 0){
    va0 = (uint)PGROUNDDOWN(va);
    pa0 = uva2ka(pgdir, (char*)va0);
    if(pa0 == 0)
      return -1;
    n = PGSIZE - (va - va0);
    if(n > len)
      n = len;
    memmove(pa0 + (va - va0), buf, n);
    len -= n;
    buf += n;
    va = va0 + PGSIZE;
  }
  return 0;
}

// Shared memory

struct shmRegion {
  uint key, size;
  int shmid;
  void *physicalAddr[SHAREDREGIONS];
  struct shmid_ds buffer;
};

struct shmRegion allRegions[SHAREDREGIONS];

int
shmget(uint key, uint size, int shmflag) {
  // as Xv6 has only single user, else lower 9 bits would be considered
  int lowerBits = shmflag & 7, permission = -1;
  
  if(lowerBits == (int)READ_SHM) {
    permission = READ_SHM;
    shmflag ^= READ_SHM;
  }
  else if(lowerBits == (int)RW_SHM) {
    permission = RW_SHM;
    shmflag ^= RW_SHM;
  } else {
    return -1;
  }
  
  int index = -1;
  // calculate no of requested pages, from entered size
  int noOfPages = (size / PGSIZE) + 1;
  // check if key already exists
  for(int i = 0; i < SHAREDREGIONS; i++) {
    if(allRegions[i].key == key) {
      if(allRegions[i].size != noOfPages) {
        return -1;
      }
      if(shmflag == (IPC_CREAT | IPC_EXCL)) {
        return -1;
      }
      int checkPerm = allRegions[i].buffer.shm_perm.mode;
      if(checkPerm == READ_SHM || checkPerm == RW_SHM) {
        if((shmflag == 0) && (key != IPC_PRIVATE)) {
          return allRegions[i].shmid;
        }
        if(shmflag == IPC_CREAT) {
          return allRegions[i].shmid;
        }
      }
      return -1;
    }
  }
  // check for first valid shared memory region, that can be allocated
  for(int i = 0; i < SHAREDREGIONS; i++) {
    if(allRegions[i].key == -1) {
      index = i;
      break;
    }
  }
  // memory regions are exhausted
  if(index == -1) {
    return -1;
  }
  if((key == IPC_PRIVATE) || (shmflag == IPC_CREAT) || (shmflag == (IPC_CREAT | IPC_EXCL))) {
    // try to allocate requested size, rounded to page size
    for(int i = 0; i < noOfPages; i++) {
      char *newPage = kalloc();
      if(newPage == 0){
        cprintf("shmget: failed to allocate a page (out of memory)\n");
        return -1;
      }
      memset(newPage, 0, PGSIZE);
      allRegions[index].physicalAddr[i] = (void *)V2P(newPage);
    }
    // mark rest of the fields in structure
    allRegions[index].size = noOfPages;
    allRegions[index].key = key;

    allRegions[index].buffer.shm_segsz = size;
    allRegions[index].buffer.shm_perm.__key = key;
    allRegions[index].buffer.shm_perm.mode = permission;

    // store creator pid
    allRegions[index].buffer.shm_cpid = myproc()->pid;

    int shmid = index;
    
    // store shmid in not yet shared region
    allRegions[index].shmid = shmid;

    return shmid;
  } else {
    return -1;
  }  
}

int getLeastvaidx(void*curr_va, struct proc *process)
{
  void* leastva = (void*)(KERNBASE - 1);
  int idx = -1;
  for(int i = 0; i < SHAREDREGIONS; i++) {
    if(process->pages[i].key != -1 && (uint)process->pages[i].virtualAddr >= (uint)curr_va && (uint)leastva >= (uint)process->pages[i].virtualAddr) {
      
      leastva = process->pages[i].virtualAddr;
      idx = i;
      
    }
  }
  
  return idx;
}

int shmdt(void* shmaddr)
{
  struct proc *process = myproc();
  void* va = (void*)0;
  uint size;
  int index,shmid;
  for(int i = 0; i < SHAREDREGIONS; i++) {
    if(process->pages[i].key != -1 && process->pages[i].virtualAddr == shmaddr) {
        //cprintf("%x\n",(uint)shmaddr);
        va =  process->pages[i].virtualAddr;
        index = i;
        shmid = process->pages[i].shmid;
        size = process->pages[index].size;
        break;
    }
  }
  if(va)
  {
    for(int i = 0; i < size; i++)
    {
      pte_t* pte = walkpgdir(process->pgdir, (void*)((uint)va + i*PGSIZE), 0);
      if(pte == 0) {
        return -1;
      }
		  *pte = 0;
    }
    process->pages[index].shmid = -1;  
    process->pages[index].key = -1;
    process->pages[index].size =  0;
    process->pages[index].virtualAddr = (void*)0;
    if(allRegions[shmid].buffer.shm_nattch > 0) {
      allRegions[shmid].buffer.shm_nattch -= 1;
    }
    allRegions[shmid].buffer.shm_lpid = process->pid;
    return 0;
  }
  else
    return -1;
  
}
  
/*
  TODO:
    1. Some more error checks.
    2. Error check for request with more than 64 pages?
*/
void*
shmat(int shmid, void* shmaddr, int shmflag)
{
  int index = -1,idx, permflag;
  uint segment,size = 0;
  void *va = (void*)HEAPLIMIT, *least_va;
  struct proc *process = myproc();
  if(shmid < 0 || shmid > 64)
  {
    return (void*)-1;
  }
  for(int i = 0; i < SHAREDREGIONS; i++) 
  {
    if(allRegions[i].shmid == shmid)
    {
      index = i;
      break;
    }  
  }
  if(index == -1)
  {
    // shmid not found
    return (void*)-1;
  }
  if(shmaddr)
  {
      uint rounded = ((uint)shmaddr & ~(SHMLBA-1));  // round down to nearest multiple of shmlba 
      if(shmflag & SHM_RND)
      {
        if(!rounded)
        {
          return (void*)-1;
        }
        va = (void*)rounded;
      }
      else
      {
        if(rounded == (uint)shmaddr) // page aligned address
        {
          va = shmaddr;
        }
      }
  }
  else
  {    
    for(int i = 0; i < SHAREDREGIONS; i++)
    {
      idx = getLeastvaidx(va,process);
      if(idx != -1)
      {
        least_va = process->pages[idx].virtualAddr;
        if((uint)va + allRegions[index].size*PGSIZE < (uint)least_va)        
          break;
        else
          va = (void*)((uint)least_va + process->pages[idx].size*PGSIZE);
      }
      else 
        break;

    }
  }
  if((uint)va + allRegions[index].size*PGSIZE >= KERNBASE)
  {
    // size exceeded
    return (void*)-1;
  }
  idx = -1;
  for(int i = 0; i < SHAREDREGIONS; i++) {
      if(process->pages[i].key != -1 && (uint)process->pages[i].virtualAddr + process->pages[i].size*PGSIZE > (uint)va && (uint)va >= (uint)process->pages[i].virtualAddr)  {
        idx = i;
        break;
      }
  }
  if (idx != -1)
  {
    if(shmflag & SHM_REMAP)
    {
      segment = (uint)process->pages[idx].virtualAddr;
      while(segment <= (uint)va + allRegions[index].size*PGSIZE)
      { 
        size = process->pages[idx].size;
        if(shmdt((void*)segment) == -1)
        {
          return (void*)-1;
        }
        idx = getLeastvaidx((void*)(segment + size*PGSIZE),process);
        if(idx == -1)
          break;
        segment = (uint)process->pages[idx].virtualAddr;
      }
    }
    else
    {
      return (void*)-1;
    }

  }
  if(shmflag & SHM_RDONLY){
    permflag = PTE_U;
  }
  else{
    permflag = PTE_W | PTE_U;
  }
  //cprintf("%x",(uint)va);
  for (int k = 0; k < allRegions[index].size; k++) {
		if(mappages(process->pgdir, (void*)((uint)va + (k*PGSIZE)), PGSIZE, (uint)allRegions[index].physicalAddr[k], permflag) < 0)
    {
      deallocuvm(process->pgdir,(uint)va,(uint)(va + allRegions[index].size));
      return (void*)-1;
    }
	}
  idx = -1;
  for(int i = 0; i < SHAREDREGIONS; i++)
  {
    if(process->pages[i].key == -1)
    {
      idx = i;
      break;
    }
  }
  if(idx != -1) {
    process->pages[idx].shmid = shmid;  
    process->pages[idx].virtualAddr = va;
    process->pages[idx].key = allRegions[index].key;
    process->pages[idx].size =  allRegions[index].size;
    process->pages[idx].perm = permflag;
    allRegions[index].buffer.shm_nattch += 1;
    allRegions[index].buffer.shm_lpid = process->pid;
  }
  else {
    return (void*)-1; // all page regions exhausted
  }
  return va;
}

/*
  TODO:
    1. Handle cmd
    2. Error checks
*/
int
shmctl(int shmid, int cmd, void *buf) {
  if(shmid < 0 || shmid > 64){
    return -1;
  }

  struct shmid_ds *buffer = (struct shmid_ds *)buf;

  int index = -1;
  for(int i = 0; i < SHAREDREGIONS; i++) {
    if(allRegions[i].shmid == shmid) {
      index = i;
      break;
    }
  }
  if(index == -1) {
    return -1;
  } else {
    int checkPerm = allRegions[index].buffer.shm_perm.mode;
    switch(cmd) {
      case IPC_SET:
        if(buffer) {
          if((buffer->shm_perm.mode == READ_SHM) || (buffer->shm_perm.mode == RW_SHM)) {
            allRegions[index].buffer.shm_perm.mode = buffer->shm_perm.mode;
            return 0;
          } else {
            return -1;
          }
        } else {
          return -1;
        }
        break;
      case SHM_STAT:
      case IPC_STAT:
        if(buffer && (checkPerm == READ_SHM || checkPerm == RW_SHM)) {
          buffer->shm_nattch = allRegions[index].buffer.shm_nattch;
          buffer->shm_segsz = allRegions[index].buffer.shm_segsz;
          buffer->shm_perm.__key = allRegions[index].buffer.shm_perm.__key;
          buffer->shm_perm.mode = checkPerm;
          buffer->shm_cpid = allRegions[index].buffer.shm_cpid;
          buffer->shm_lpid = allRegions[index].buffer.shm_lpid;
          return 0;
        } else {
          return -1;
        }
        break;
      case IPC_RMID:
        if(allRegions[index].buffer.shm_nattch == 0) {
          for(int i = 0; i < allRegions[index].size; i++) {
            char *addr = (char *)P2V(allRegions[index].physicalAddr[i]);
            kfree(addr);
            allRegions[index].physicalAddr[i] = (void *)0;
          }
          allRegions[index].size = 0;
          allRegions[index].key = allRegions[index].shmid = -1;
          allRegions[index].buffer.shm_nattch = 0;
          allRegions[index].buffer.shm_segsz = 0;
          allRegions[index].buffer.shm_perm.__key = -1;
          allRegions[index].buffer.shm_perm.mode = 0;
          allRegions[index].buffer.shm_cpid = -1;
          allRegions[index].buffer.shm_lpid = -1;
          return 0;
        } else {
          return -1;
        }
        break;
      default:
        return -1;
        break;
    }
  } 
}

void
sharedMemoryInit(void) {
  for(int i = 0; i < SHAREDREGIONS; i++) {
    allRegions[i].key = allRegions[i].shmid = -1;
    allRegions[i].size = 0;
    allRegions[i].buffer.shm_nattch = 0;
    allRegions[i].buffer.shm_segsz = 0;
    allRegions[i].buffer.shm_perm.__key = -1;
    allRegions[i].buffer.shm_perm.mode = 0;
    allRegions[i].buffer.shm_cpid = -1;
    allRegions[i].buffer.shm_lpid = -1;
    for(int j = 0; j < SHAREDREGIONS; j++) {
      allRegions[i].physicalAddr[j] = (void *)0;
    }
  }
}

int
getShmidIndex(int shmid) {
  int index = -1;
  for(int i = 0; i < SHAREDREGIONS; i++) {
    if(allRegions[i].shmid == shmid) {
      index = i;
      break;
    }
  }
  return index;
}

void mappagesWrapper(struct proc *process, int shmIndex, int index) {
  for(int i = 0; i < process->pages[index].size; i++) {
    uint va = (uint)process->pages[index].virtualAddr;
    if(mappages(process->pgdir, (void*)(va + (i * PGSIZE)), PGSIZE, (uint)allRegions[shmIndex].physicalAddr[i], process->pages[index].perm) < 0) {
      deallocuvm(process->pgdir, va, (uint)(va + allRegions[shmIndex].size));
      return;
    }
  }
}

void shmdtWrapper(void *addr) {
  shmdt(addr);
}

//PAGEBREAK!
// Blank page.
//PAGEBREAK!
// Blank page.
//PAGEBREAK!
// Blank page.

