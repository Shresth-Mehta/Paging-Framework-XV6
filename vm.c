#include "param.h"
#include "types.h"
#include "defs.h"
#include "x86.h"
#include "memlayout.h"
#include "mmu.h"
#include "proc.h"
#include "elf.h"

#define BUF_SIZE PGSIZE/4

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
walkpgdir(pde_t *pgdir, const void *va, int alloc){
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
mappages(pde_t *pgdir, void *va, uint size, uint pa, int perm){
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
setupkvm(void){
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
kvmalloc(void){
  kpgdir = setupkvm();
  switchkvm();
}

// Switch h/w page table register to the kernel-only page table,
// for when no process is running.
void
switchkvm(void){
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


int 
accessedBit(char *va){
  struct proc *proc = myproc();
  uint flag;
  pte_t *pte = walkpgdir(proc->pgdir,(void*)va,0);
  if(!pte)
    panic("pte not found");
  flag = (*pte) & PTE_A;
  (*pte) &= ~PTE_A;
  return flag;
}


struct freepg *writePageToSwapFile(char *va){

#if NFU

  struct proc *proc = myproc();
  int i,j;
  uint maxIndex = -1;
  uint maxAge = 0;

  struct freepg *candidate;

  for(i=0; i<MAX_PSYC_PAGES; i++){
    if(proc->swap_space_pages[i].va == (char*)0xffffffff){

      for(j=0; j<MAX_PSYC_PAGES; j++){
        if(proc->free_pages[j].va != (char*)0xffffffff){
          if(proc->free_pages[j].age > maxAge){
              maxAge = proc->free_pages[j].age;
              maxIndex = j;
            }
        }
      }
      if(maxIndex == -1)
        panic("nfuWrite: no free page to swap");
      candidate = &proc->free_pages[maxIndex];

      pte_t *pte1 = walkpgdir(proc->pgdir, (void*)candidate->va, 0);
      if(!pte1)
        panic("writePageToSwapFile: nfuWrite pte1 is empty");
      
      acquire(&tickslock);
      if((*pte1) && PTE_A){
        candidate->age+=1;;
        *pte1 &= ~PTE_A;
      }
      release(&tickslock);
      proc->swap_space_pages[i].va = candidate->va;

      int test_num = writeToSwapFile(proc, (char*)PTE_ADDR(candidate->va), i * PGSIZE, PGSIZE);
      if (test_num == 0)
        return 0;
      kfree((char*)PTE_ADDR(P2V_WO(*walkpgdir(proc->pgdir, candidate->va, 0))));
      *pte1 = PTE_W | PTE_U | PTE_PG;
      proc->page_swapped_count+=1;
      proc->swap_file_pages+=1;

      lcr3(V2P(proc->pgdir));
      candidate->va = va;
      return candidate;
    }
  }
  panic("writePageToSwapFile: NFU no slot for swapped page");

#elif SCFIFO

  struct proc *proc = myproc();
  int i;
  struct freepg *itr, *init_itr;
  for(i=0; i<MAX_PSYC_PAGES; i++){
    if(proc->swap_space_pages[i].va == (char*)0xffffffff){

      if(proc->head == 0 || proc->head->next == 0)
        panic("scFifoWrite: not enough phy memory pages");
      itr = proc->tail;
      init_itr = proc->tail;
      do{
      proc->tail = proc->tail->prev;
      proc->tail->next = 0;
      itr->prev = 0;
      itr->next = proc->head;
      proc->head->prev = itr;
      proc->head = itr;
      itr = proc->tail;
      }while(accessedBit(proc->head->va) && itr !=init_itr);
    
      proc->swap_space_pages[i].va = proc->head->va;
      int num = writeToSwapFile(proc, (char*)PTE_ADDR(proc->head->va), i * PGSIZE, PGSIZE);
      if(num == 0)
        return 0;
      pte_t *pte1 = walkpgdir(proc->pgdir, (void*)proc->head->va, 0);
      if(!*pte1)
        panic("writePageToSwapFile: pte1 not found");
      
      kfree((char*)PTE_ADDR(P2V_WO(*walkpgdir(proc->pgdir, proc->head->va, 0))));
      *pte1 = PTE_W | PTE_U | PTE_PG;
      ++proc->page_swapped_count;
      ++proc->swap_file_pages;

      lcr3(V2P(proc->pgdir));
      proc->head->va = va;
      return proc->head;
    }
  }
  panic("writePageToSwapFile: No free page available in the swap space");

#elif FIFO

  struct proc *proc = myproc();
  int i;
  struct freepg *temp, *last;
  for(i=0;i<MAX_PSYC_PAGES;i++){
    if(proc->swap_space_pages[i].va == (char*)0xffffffff){

      temp = proc->head;
      if(temp == 0)
        panic("fifoWrite: proc->head is NULL");
      if(temp->next == 0)
        panic("fifoWrite: only one page in phy mem");
      while(temp->next->next!=0)
        temp = temp->next;
      last = temp->next;
      temp->next = 0;

      proc->swap_space_pages[i].va = last->va;
      int num = writeToSwapFile(proc,(char*)PTE_ADDR(last->va),i*PGSIZE, PGSIZE);
      if(num == 0)
        return 0;
      pte_t *pte1 = walkpgdir(proc->pgdir, (void*)last->va, 0);
      if(!*pte1)
        panic("writePageToSwapFile: pte1 is empty");
      kfree((char*)PTE_ADDR(P2V_WO(*walkpgdir(proc->pgdir, last->va, 0))));
      *pte1 = PTE_W | PTE_U | PTE_PG;
      ++proc->page_swapped_count;
      ++proc->swap_file_pages;
      lcr3(V2P(proc->pgdir));
      return last;
    }  
  }
  panic("writePagesToSwapFile: FIFO no slot for swapped page");
  
#endif

  return 0;  
}


void recordNewPage(char *va){

  struct proc *proc = myproc();

  #if NFU 

    int i;
    for(i=0; i<MAX_PSYC_PAGES; i++)
    if(proc->free_pages[i].va == (char*)0xffffffff){
      proc->free_pages[i].va = va;
      proc->main_mem_pages++;
      return;
    }
    panic("recordNewPage: no free page found in main memory");
  
  #elif SCFIFO

    int i;
    for (i = 0; i < MAX_PSYC_PAGES; i++)
      if (proc->free_pages[i].va == (char*)0xffffffff){
        proc->free_pages[i].va = va;
        proc->free_pages[i].next = proc->head;
        proc->free_pages[i].prev = 0;
        if(proc->head != 0)
          proc->head->prev = &proc->free_pages[i];
        else
          proc->tail = &proc->free_pages[i];
        proc->head = &proc->free_pages[i];
        proc->main_mem_pages++;
        return;
      }
    cprintf("panic follows, pid:%d, name:%s\n", proc->pid, proc->name);
    panic("recordNewPage: no free pages"); 
  
  #elif FIFO 
  
    int i;
    for(i=0;i<MAX_PSYC_PAGES;i++)
      if(proc->free_pages[i].va == (char*)0xffffffff){
        proc->free_pages[i].va = va;
        proc->free_pages[i].next = proc->head;
        proc->head = &proc->free_pages[i];
        proc->main_mem_pages++;
        return;
      }
    cprintf("panic follows, pid:%d, name:%s\n", proc->pid, proc->name);
    panic("recordNewPage: no free pages"); 
  
  #endif
}

// Allocate page tables and physical memory to grow process from oldsz to
// newsz, which need not be page aligned.  Returns new size or 0 on error.
int
allocuvm(pde_t *pgdir, uint oldsz, uint newsz)
{
  char *mem;
  uint a;

  if(newsz >= KERNBASE)
    return 0;
  if(newsz < oldsz)
    return oldsz;

  a = PGROUNDUP(oldsz);
  
  #ifndef NONE
    struct proc *proc = myproc();
  #endif

  for(; a < newsz; a += PGSIZE){
  #ifndef NONE
    struct freepg *last;
    uint newpage = 1;
    if(proc->main_mem_pages >= MAX_PSYC_PAGES && proc->pid > 2){
      last = writePageToSwapFile((char*)a);
      if(last == 0)
        panic("Cannot write to swap file :: allocuvm");
        
      #if FIFO
      //cprintf("should be called fifo part\n");
        last->va = (char*)a;
        last->next = proc->head;
        proc->head = last;
      #endif
      newpage = 0;
    }
  #endif

    mem = kalloc();
    if(mem == 0){
      cprintf("allocuvm out of memory\n");
      deallocuvm(pgdir, newsz, oldsz);
      return 0;
    }
    #ifndef NONE
      if(newpage){
        recordNewPage((char*)a);
      }
    #endif
    memset(mem, 0, PGSIZE);
    if(mappages(pgdir, (char*)a, PGSIZE, V2P(mem), PTE_W|PTE_U) < 0){
      cprintf("allocuvm out of memory (2)\n");
      deallocuvm(pgdir, newsz, oldsz);
      kfree(mem);
      return 0;
    }
  }
  //cprintf("\ncalled allocuvm: %d\n",newsz);
  return newsz;
}


// TODO =-=-=-=-====-=-=-=-=-=-=-=-= 
void swapPages(uint addr){

  struct proc *proc = myproc();
  if (proc->pid <2) {
    return;
  }

  #if NFU 

    int i,j;
    uint maxIndex = -1;
    uint maxAge = 0;
    char buf[BUF_SIZE];
    pte_t *pte1, *pte2;
    struct freepg *candidate;
    for (j = 0; j < MAX_PSYC_PAGES; j++){
      if (proc->free_pages[j].va != (char*)0xffffffff){
        if (proc->free_pages[j].age > maxAge){
          maxAge = proc-> free_pages[j].age;
          maxIndex = j;
        }
      }
    }
    if(maxIndex == -1)
      panic("nfuSwap: no free page to swap");
    candidate = &proc->free_pages[maxIndex];
    pte1 = walkpgdir(proc->pgdir, (void*)candidate->va, 0);
    if(!*pte1)  
      panic("nfuSwap: pte1 is empty");
    acquire(&tickslock);
    if((*pte1) & PTE_A){
      ++candidate->age;
      *pte1 &= ~PTE_A;
    }
    release(&tickslock);
    for (i = 0; i < MAX_PSYC_PAGES; i++){
      if (proc->swap_space_pages[i].va == (char*)PTE_ADDR(addr)){
        proc->swap_space_pages[i].va = candidate->va;
        pte2 = walkpgdir(proc->pgdir, (void*)addr, 0);
        if (!*pte2)
          panic("nfuSwap: pte2 is empty");
        *pte2 = PTE_ADDR(*pte1) | PTE_U | PTE_W | PTE_P;// access bit is zeroed...

        for (j = 0; j < 4; j++) {
          int loc = (i * PGSIZE) + ((PGSIZE / 4) * j);
          int addroffset = ((PGSIZE / 4) * j);
          memset(buf, 0, BUF_SIZE);
          readFromSwapFile(proc, buf, loc, BUF_SIZE);
          writeToSwapFile(proc, (char*)(P2V_WO(PTE_ADDR(*pte1)) + addroffset), loc, BUF_SIZE);
          memmove((void*)(PTE_ADDR(addr) + addroffset), (void*)buf, BUF_SIZE);
        }
        *pte1 = PTE_U | PTE_W | PTE_PG;
        candidate->va = (char*)PTE_ADDR(addr);
        candidate->age = 0;
        lcr3(V2P(proc->pgdir));
        proc->page_swapped_count++;
        return;
      }      
    }
    panic("nfuSwap: no slot for swapped page");

  #elif SCFIFO

    int i,j;
    char buf[BUF_SIZE];
    pte_t *pte1, *pte2;
    struct freepg *itr, *init_itr;

    if (proc->head == 0)
      panic("scSwap: proc->head is NULL");
    if (proc->head->next == 0)
      panic("scSwap: single page in phys mem");

    itr = proc->tail;
    init_itr = proc->tail;
    do{
      //move mover from tail to head
      proc->tail = proc->tail->prev;
      proc->tail->next = 0;
      itr->prev = 0;
      itr->next = proc->head;
      proc->head->prev = itr;
      proc->head = itr;
      itr = proc->tail;
    }while(accessedBit(proc->head->va) && itr != init_itr);

    pte1 = walkpgdir(proc->pgdir, (void*)proc->head->va, 0);
    if (!*pte1)
      panic("swapFile: SCFIFO pte1 is empty");

    //find a swap file page descriptor slot
    for (i = 0; i < MAX_PSYC_PAGES; i++){
      if (proc->swap_space_pages[i].va == (char*)PTE_ADDR(addr)){
        proc->swap_space_pages[i].va = proc->head->va;
        pte2 = walkpgdir(proc->pgdir, (void*)addr, 0);
        if (!*pte2)
          panic("swapFile: SCFIFO pte2 is empty");
        *pte2 = PTE_ADDR(*pte1) | PTE_U | PTE_W | PTE_P;// access bit is zeroed...

        for (j = 0; j < 4; j++) {
          int loc = (i * PGSIZE) + ((PGSIZE / 4) * j);
          int addroffset = ((PGSIZE / 4) * j);
          memset(buf, 0, BUF_SIZE);
          readFromSwapFile(proc, buf, loc, BUF_SIZE);
          writeToSwapFile(proc, (char*)(P2V_WO(PTE_ADDR(*pte1)) + addroffset), loc, BUF_SIZE);
          memmove((void*)(PTE_ADDR(addr) + addroffset), (void*)buf, BUF_SIZE);
        }
        *pte1 = PTE_U | PTE_W | PTE_PG;
        proc->head->va = (char*)PTE_ADDR(addr);
        lcr3(V2P(proc->pgdir));
        proc->page_swapped_count++;
        return; 
    }
    panic("scSwap: SCFIFO no slot for swapped page");

  #elif FIFO

    int i,j;
    char buffer[BUF_SIZE];
    pte_t *pte1, *pte2;

    struct freepg *temp = proc->head;
    struct freepg *last;

    if(temp == 0)
      panic("fifoSwap: proc->head is NULL");
    if (temp->next == 0)
      panic("fifoSwap: single page in phys mem");
    // find the before-last link in the used pages list
    while (temp->next->next != 0)
      temp = temp->next;
    last = temp->next;
    temp->next = 0; 

    pte1 = walkpgdir(proc->pgdir,(void*)last->va,0);  
    if(!*pte1)
      panic("swapFile: FIFO pte1 is empty");
    for(i = 0;i<MAX_PSYC_PAGES;i++)
      if(proc->swap_space_pages[i].va == (char*)PTE_ADDR(addr)){
        proc->swap_space_pages[i].va = last->va;
        pte2 = walkpgdir(proc->pgdir, (void*)addr, 0);
        if(!*pte2)
          panic("swapFile: FIFO pte2 is empty");
        
        *pte2 = PTE_ADDR(*pte1) | PTE_U | PTE_W | PTE_P;
        for(j=0;j<4;j++){
          int loc = (i * PGSIZE) + ((PGSIZE / 4) * j);
          int addroffset = ((PGSIZE / 4) * j);
          memset(buffer,0,BUF_SIZE);
          readFromSwapFile(proc,buffer,loc,BUF_SIZE);
          writeToSwapFile(proc, (char*)(P2V_WO(PTE_ADDR(*pte1)) + addroffset), loc, BUF_SIZE);
          memmove((void*)(PTE_ADDR(addr) + addroffset), (void*)buffer, BUF_SIZE);
        }
        *pte1 = PTE_U | PTE_W | PTE_PG;
        last->next = proc->head;
        proc->head = last;
        last->va = (char*)PTE_ADDR(addr);
        lcr3(V2P(proc->pgdir));
        proc->page_swapped_count++;
        return; 
      }
    panic("Problem in swappages");

  #endif
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
  int i;
  struct proc* proc = myproc();
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
      if(proc->pgdir == pgdir){
#ifndef NONE
        for(i=0; i<MAX_PSYC_PAGES; i++){
          if(proc->free_pages[i].va == (char*)a){
            proc->free_pages[i].va = (char*)0xffffffff;
#if FIFO
            if (proc->head == &proc->free_pages[i])
              proc->head = proc->free_pages[i].next;
            else {
              struct freepg *last = proc->head;
              while (last->next != &proc->free_pages[i])
              last = last->next;
              last->next = proc->free_pages[i].next;
            }
            proc->free_pages[i].next = 0;
#elif SCFIFO
            if (proc->head == &proc->free_pages[i]){
              proc->head = proc->free_pages[i].next;
              if(proc->head != 0)
                proc->head->prev = 0;
              proc->free_pages[i].next = 0;
              proc->free_pages[i].prev = 0;
            }
            if (proc->tail == &proc->free_pages[i]){
              proc->tail = proc->free_pages[i].prev;
              if(proc->tail != 0)// should allways be true but lets be extra safe...
                proc->tail->next = 0;
              proc->free_pages[i].next = 0;
              proc->free_pages[i].prev = 0;

            }
            struct freepg *last = proc->head;
            while (last->next != 0 && last->next != &proc->free_pages[i]){
              last = last->next;
            }
            last->next = proc->free_pages[i].next;
            if (proc->free_pages[i].next != 0){
              proc->free_pages[i].next->prev = last;
            }

#endif     
            proc->main_mem_pages--;
          }    
        }
#endif
      }
      char *v = P2V(pa);
      kfree(v);
      *pte = 0;
    }
    else if((*pte & PTE_PG) && proc->pgdir == pgdir){
      for(i=0; i<MAX_PSYC_PAGES; i++){
        if(proc->swap_space_pages[i].va == (char*)a){
          proc->swap_space_pages[i].va = (char*) 0xffffffff;
          proc->swap_space_pages[i].age = 0;
          proc->swap_space_pages[i].swaploc = 0;
          proc->swap_file_pages--;     
        }
      }
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
  deallocuvm(pgdir, KERNBASE, 0);
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
    if(!(*pte & PTE_P) && !(*pte & PTE_PG))
      panic("copyuvm: page not present");
    if (*pte & PTE_PG) {
      // cprintf("copyuvm PTR_PG\n"); // TODO delete
      pte = walkpgdir(d, (void*) i, 1);
      *pte = PTE_U | PTE_W | PTE_PG;
      continue;
    }
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

//PAGEBREAK!
// Blank page.
//PAGEBREAK!
// Blank page.
//PAGEBREAK!
// Blank page.

