#include "types.h"
#include "param.h"
#include "memlayout.h"
#include "mmu.h"
#include "proc.h"
#include "defs.h"
#include "x86.h"
#include "elf.h"

int
exec(char *path, char **argv){
  
  //cprintf("called this but not that in exec.c lolololl");
  char *s, *last;
  int i, off;
  uint argc, sz, sp, ustack[3+MAXARG+1];
  struct elfhdr elf;
  struct inode *ip;
  struct proghdr ph;
  pde_t *pgdir, *oldpgdir;
  struct proc *proc = myproc();


  #ifndef NONE
  //cprintf("called???\n");
  int mem_pages = proc->main_mem_pages;
  int swap_file_pages = proc->swap_file_pages;
  int page_fault_count = proc->page_fault_count;
  int paged_out_count = proc->page_swapped_count;
  struct freepg temp_free_pages[MAX_PSYC_PAGES];
  struct discpg temp_swap_space_pages[MAX_PSYC_PAGES];
  struct freepg *head = proc->head;
  struct freepg *tail = proc->tail;
  #endif

  begin_op();
  if((ip = namei(path)) == 0){
    end_op();
    cprintf("exec: fail\n");
    return -1;
  }
  ilock(ip);
  pgdir = 0;

  // Check ELF header
  if(readi(ip, (char*)&elf, 0, sizeof(elf)) != sizeof(elf))
    goto bad;
  if(elf.magic != ELF_MAGIC)
    goto bad;

  if((pgdir = setupkvm()) == 0)
    goto bad;
  //cprintf("called in exec: procdump()");
  //procdump(myproc());

  //Clone all the meta-data of a zombie process and removing it from the proc structure
  #ifndef NONE
    for(int i=0;i < MAX_PSYC_PAGES; i++){
      temp_free_pages[i].va = proc->free_pages[i].va;
      proc->free_pages[i].va = (char*)0xffffffff;
      temp_free_pages[i].next = proc->free_pages[i].next;
      proc->free_pages[i].next = 0;
      temp_free_pages[i].prev = proc->free_pages[i].prev;
      proc->free_pages[i].prev = 0;
      temp_free_pages[i].age = proc->free_pages[i].age;
      proc->free_pages[i].age = 0;
      temp_swap_space_pages[i].age = proc->swap_space_pages[i].age;
      proc->swap_space_pages[i].age = 0;
      temp_swap_space_pages[i].va = proc->swap_space_pages[i].va;
      proc->swap_space_pages[i].va = (char*)0xffffffff;
      temp_swap_space_pages[i].swaploc = proc->swap_space_pages[i].swaploc;
      proc->swap_space_pages[i].swaploc = 0;
    }

    proc->main_mem_pages = 0;
    proc->swap_file_pages = 0;
    proc->page_fault_count = 0;
    proc->page_swapped_count = 0;
    proc->head = 0;
    proc->tail = 0;
  #endif
  // Load program into memory.
  sz = 0;
  for(i=0, off=elf.phoff; i<elf.phnum; i++, off+=sizeof(ph)){
    if(readi(ip, (char*)&ph, off, sizeof(ph)) != sizeof(ph))
      goto bad;
    if(ph.type != ELF_PROG_LOAD)
      continue;
    if(ph.memsz < ph.filesz)
      goto bad;
    if(ph.vaddr + ph.memsz < ph.vaddr)
      goto bad;
    if((sz = allocuvm(pgdir, sz, ph.vaddr + ph.memsz)) == 0)
      goto bad;
    if(ph.vaddr % PGSIZE != 0)
      goto bad;
    if(loaduvm(pgdir, (char*)ph.vaddr, ip, ph.off, ph.filesz) < 0)
      goto bad;
  }
  iunlockput(ip);
  end_op();
  ip = 0;

  // Allocate two pages at the next page boundary.
  // Make the first inaccessible.  Use the second as the user stack.
  sz = PGROUNDUP(sz);
  if((sz = allocuvm(pgdir, sz, sz + 2*PGSIZE)) == 0)
    goto bad;
  clearpteu(pgdir, (char*)(sz - 2*PGSIZE));
  sp = sz;

  // Push argument strings, prepare rest of stack in ustack.
  for(argc = 0; argv[argc]; argc++) {
    if(argc >= MAXARG)
      goto bad;
    sp = (sp - (strlen(argv[argc]) + 1)) & ~3;
    if(copyout(pgdir, sp, argv[argc], strlen(argv[argc]) + 1) < 0)
      goto bad;
    ustack[3+argc] = sp;
  }
  ustack[3+argc] = 0;

  ustack[0] = 0xffffffff;  // fake return PC
  ustack[1] = argc;
  ustack[2] = sp - (argc+1)*4;  // argv pointer

  sp -= (3+argc+1) * 4;
  if(copyout(pgdir, sp, ustack, (3+argc+1)*4) < 0)
    goto bad;

  // Save program name for debugging.
  for(last=s=path; *s; s++)
    if(*s == '/')
      last = s+1;
  safestrcpy(proc->name, last, sizeof(proc->name));

  // Commit to the user image.
  oldpgdir = proc->pgdir;
  proc->pgdir = pgdir;
  proc->sz = sz;
  proc->tf->eip = elf.entry;  // main
  proc->tf->esp = sp;

  if(proc->pid > 2){
    createSwapFile(proc);
  }

  switchuvm(proc);
  freevm(oldpgdir);
  return 0;

 bad:
  if(pgdir)
    freevm(pgdir);
  if(ip){
    iunlockput(ip);
    end_op();
  }
  
  #ifndef NONE
    proc->main_mem_pages = mem_pages;
    proc->swap_file_pages = swap_file_pages;
    proc->page_fault_count = page_fault_count;
    proc->page_swapped_count = paged_out_count;
    proc->head = head;
    proc->tail = tail;
    for (i = 0; i < MAX_PSYC_PAGES; i++) {
      proc->free_pages[i].va = temp_free_pages[i].va;
      proc->free_pages[i].next = temp_free_pages[i].next;
      proc->free_pages[i].prev = temp_free_pages[i].prev;
      proc->free_pages[i].age = temp_free_pages[i].age;
      proc->swap_space_pages[i].age = temp_swap_space_pages[i].age;
      proc->swap_space_pages[i].va = temp_swap_space_pages[i].va;
      proc->swap_space_pages[i].swaploc = temp_swap_space_pages[i].swaploc;
    }
  #endif
  
  return -1;
}
