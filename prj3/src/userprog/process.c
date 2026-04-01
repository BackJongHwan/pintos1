#include "userprog/process.h"
#include <debug.h>
#include <inttypes.h>
#include <round.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "userprog/gdt.h"
#include "userprog/pagedir.h"
#include "userprog/tss.h"
#include "filesys/directory.h"
#include "filesys/file.h"
#include "filesys/filesys.h"
#include "threads/flags.h"
#include "threads/init.h"
#include "threads/interrupt.h"
#include "threads/palloc.h"
#include "threads/thread.h"
#include "threads/vaddr.h"
//append list structure, string
#include "lib/kernel/list.h"
#include "threads/malloc.h"  
#include "userprog/syscall.h"

static thread_func start_process NO_RETURN;
static bool load (const char *cmdline, void (**eip) (void), void **esp);

/* Starts a new thread running a user program loaded from
   FILENAME.  The new thread may be scheduled (and may even exit)
   before process_execute() returns.  Returns the new process'/
   thread id, or TID_ERROR if the thread cannot be created. */
tid_t
process_execute (const char *file_name) 
{
  // printf("%s\n", file_name);
  char *fn_copy;
  tid_t tid;
  char *save_ptr;
  // printf("file_name: %s\n", file_name);
  /* Make a copy of FILE_NAME.
     Otherwise there's a race between the caller and load(). */
  fn_copy = palloc_get_page (0);
  if (fn_copy == NULL)
    return TID_ERROR;
  strlcpy(fn_copy, file_name, PGSIZE);

  char *program_name = palloc_get_page(0);
  if (program_name == NULL) {
    palloc_free_page(fn_copy);
    return TID_ERROR;
  }
  char *original_ptr = program_name;
  strlcpy(program_name, file_name, PGSIZE);  // file_name 복사본 만들기
// 
  program_name = strtok_r(program_name, " ",&save_ptr);
  struct file *f = filesys_open(program_name);
  if(f == NULL){
    // thread_current()->exit_status = -1;
    palloc_free_page(fn_copy);
    palloc_free_page(original_ptr);
    return TID_ERROR;
  }

  //process execute될 때 쓰기 금지 그 파일에 대한 쓰기 금지
  file_deny_write(f);
  //parent process가 자식 process를 실행할 때 그 file을 저장함
  thread_current()->parent_exec_file = f;

  /* Create a new thread to execute FILE_NAME. */
  tid = thread_create (program_name, PRI_DEFAULT, start_process, fn_copy);
  if (tid == TID_ERROR){
    //thread가 안 만들어지면 복구
    file_allow_write(f);
    palloc_free_page (fn_copy);
    palloc_free_page(original_ptr);
    return TID_ERROR;
  }


  //tid로 child thread찾고 semaphore를 down함  process wait에서 사용한 것임
  struct thread *cur = thread_current();
  struct thread *child_thread = NULL;
  struct list_elem *e;
  for(e = list_begin(&(cur->children_list)); e != list_end(&(cur->children_list));e = list_next(e)){
    child_thread = list_entry(e, struct thread, child_elem);
    if(child_thread->tid == tid){
      break;
    }
    child_thread = NULL;
  }
  
  if (child_thread == NULL) {
    palloc_free_page(original_ptr);
    return TID_ERROR;
  }
   
  sema_down(&child_thread->load_sema);

  if(!child_thread->load_flag){
    palloc_free_page(original_ptr);
    return -1;
  }

  palloc_free_page(original_ptr);

  return tid;
}

/* A thread function that loads a user process and starts it
   running. */
static void
start_process (void *file_name_)
{
  // printf("start_process\n");
  char *file_name = file_name_;
  struct intr_frame if_;
  bool success;
  struct thread *t = thread_current();

  /* Initialize interrupt frame and load executable. */
  memset (&if_, 0, sizeof if_);
  if_.gs = if_.fs = if_.es = if_.ds = if_.ss = SEL_UDSEG;
  if_.cs = SEL_UCSEG;
  if_.eflags = FLAG_IF | FLAG_MBS;

  // sema_down(&t->parent->load_sema);

  success = load (file_name, &if_.eip, &if_.esp);
  //현재의 thread가 실행하고 있는 파일에 대한 정보를 저장
  t->now_exec_file = file_reopen(t->parent->parent_exec_file); // 부모의 파일을 복사
  //load를 완료하고 parent process가 실행 할 수 있도록 sema up을 함.
  t->load_flag = true;
  sema_up(&t->load_sema);
  /* If load failed, quit. */
  palloc_free_page (file_name);
  if (!success) {
    t->load_flag = false;
    t->exit_status = -1;
    thread_exit ();
  }

  /* Start the user process by simulating a return from an
     interrupt, implemented by intr_exit (in
     threads/intr-stubs.S).  Because intr_exit takes all of its
     arguments on the stack in the form of a `struct intr_frame',
     we just point the stack pointer (%esp) to our stack frame
     and jump to it. */
  asm volatile ("movl %0, %%esp; jmp intr_exit" : : "g" (&if_) : "memory");
  NOT_REACHED ();
}

/* Waits for thread TID to die and returns its exit status.  If
   it was terminated by the kernel (i.e. killed due to an
   exception), returns -1.  If TID is invalid or if it was not a
   child of the calling process, or if process_wait() has already
   been successfully called for the given TID, returns -1
   immediately, without waiting.

   This function will be implemented in problem 2-2.  For now, it
   does nothing. */
int
process_wait (tid_t child_tid UNUSED) 
{
  // 
  struct thread *cur = thread_current();
  struct thread *child_thread = NULL;
  struct list_elem *e;
  int status = -1;
  for(e = list_begin(&(cur->children_list)); e != list_end(&(cur->children_list));e = list_next(e)){
    child_thread = list_entry(e, struct thread, child_elem);
    if(child_thread->tid == child_tid){
      break;
    }
    child_thread = NULL;
  }
  if (child_thread == NULL) {
    return -1;
  }

  sema_down(&child_thread->wait); //waiting for child_thread is dying
  status = child_thread->exit_status;

  //자식 process가 종료될 때 그 파일에 대한 쓰기접근을 허용함
  if (cur->parent_exec_file != NULL) {
      file_allow_write(cur->parent_exec_file);
  }
  list_remove(e);
  //after remove child in child list, accept child_thread terminate
  sema_up(&child_thread->exit);
  
  return status;
}

/* Free the current process's resources. */
void
process_exit (void)
{
  struct thread *cur = thread_current ();
  uint32_t *pd;

  /* Destroy the current process's page directory and switch back
     to the kernel-only page directory. */
  pd = cur->pagedir;
  if (pd != NULL) 
    {
      /* Correct ordering here is crucial.  We must set
         cur->pagedir to NULL before switching page directories,
         so that a timer interrupt can't switch back to the
         process page directory.  We must activate the base page
         directory before destroying the process's page
         directory, or our active page directory will be one
         that's been freed (and cleared). */
      cur->pagedir = NULL;
      pagedir_activate (NULL);
      pagedir_destroy (pd);

      //when process exit, close all the open files.
      for(int i = 2; i < FD_MAX; i++){
        if(cur->fd_table[i])
          file_close(cur->fd_table[i]);
      }
    }
}

/* Sets up the CPU for running user code in the current
   thread.
   This function is called on every context switch. */
void
process_activate (void)
{
  struct thread *t = thread_current ();

  /* Activate thread's page tables. */
  pagedir_activate (t->pagedir);

  /* Set thread's kernel stack for use in processing
     interrupts. */
  tss_update ();
}

/* We load ELF binaries.  The following definitions are taken
   from the ELF specification, [ELF1], more-or-less verbatim.  */

/* ELF types.  See [ELF1] 1-2. */
typedef uint32_t Elf32_Word, Elf32_Addr, Elf32_Off;
typedef uint16_t Elf32_Half;

/* For use with ELF types in printf(). */
#define PE32Wx PRIx32   /* Print Elf32_Word in hexadecimal. */
#define PE32Ax PRIx32   /* Print Elf32_Addr in hexadecimal. */
#define PE32Ox PRIx32   /* Print Elf32_Off in hexadecimal. */
#define PE32Hx PRIx16   /* Print Elf32_Half in hexadecimal. */

/* Executable header.  See [ELF1] 1-4 to 1-8.
   This appears at the very beginning of an ELF binary. */
struct Elf32_Ehdr
  {
    unsigned char e_ident[16];
    Elf32_Half    e_type;
    Elf32_Half    e_machine;
    Elf32_Word    e_version;
    Elf32_Addr    e_entry;
    Elf32_Off     e_phoff;
    Elf32_Off     e_shoff;
    Elf32_Word    e_flags;
    Elf32_Half    e_ehsize;
    Elf32_Half    e_phentsize;
    Elf32_Half    e_phnum;
    Elf32_Half    e_shentsize;
    Elf32_Half    e_shnum;
    Elf32_Half    e_shstrndx;
  };

/* Program header.  See [ELF1] 2-2 to 2-4.
   There are e_phnum of these, starting at file offset e_phoff
   (see [ELF1] 1-6). */
struct Elf32_Phdr
  {
    Elf32_Word p_type;
    Elf32_Off  p_offset;
    Elf32_Addr p_vaddr;
    Elf32_Addr p_paddr;
    Elf32_Word p_filesz;
    Elf32_Word p_memsz;
    Elf32_Word p_flags;
    Elf32_Word p_align;
  };


/* Values for p_type.  See [ELF1] 2-3. */
#define PT_NULL    0            /* Ignore. */
#define PT_LOAD    1            /* Loadable segment. */
#define PT_DYNAMIC 2            /* Dynamic linking info. */
#define PT_INTERP  3            /* Name of dynamic loader. */
#define PT_NOTE    4            /* Auxiliary info. */
#define PT_SHLIB   5            /* Reserved. */
#define PT_PHDR    6            /* Program header table. */
#define PT_STACK   0x6474e551   /* Stack segment. */

/* Flags for p_flags.  See [ELF3] 2-3 and 2-4. */
#define PF_X 1          /* Executable. */
#define PF_W 2          /* Writable. */
#define PF_R 4          /* Readable. */

static bool setup_stack (void **esp);
static bool validate_segment (const struct Elf32_Phdr *, struct file *);
static bool load_segment (struct file *file, off_t ofs, uint8_t *upage,
                          uint32_t read_bytes, uint32_t zero_bytes,
                          bool writable);

/* Loads an ELF executable from FILE_NAME into the current thread.
   Stores the executable's entry point into *EIP
   and its initial stack pointer into *ESP.
   Returns true if successful, false otherwise. */
bool
load (const char *file_name, void (**eip) (void), void **esp) 
{
  // printf("Starting load function\n");
  struct thread *t = thread_current ();
  struct Elf32_Ehdr ehdr;
  struct file *file = NULL;
  off_t file_ofs;
  bool success = false;
  // printf("%s\n", file_name);

  /* Allocate and activate page directory. */
  t->pagedir = pagedir_create ();
  if (t->pagedir == NULL) 
    goto done;
  process_activate ();

  //TODO: parse file name
  //make list for agrv
  int i;
  struct list cmd_list;
  list_init(&cmd_list);
  char *save_ptr;
  int argc = 0;
  struct arg_item *arg;
  char *copy_file_name = palloc_get_page(0);
  if (copy_file_name == NULL) {
    goto done;
  }
  char *original = copy_file_name;
  strlcpy(copy_file_name, file_name, PGSIZE);       // 할당된 메모리에 문자열 복사
  // printf("%s\n", copy_file_name);
  char *token = strtok_r(copy_file_name, " ", &save_ptr);

  while (token != NULL) {
    arg = palloc_get_page(0);
    if (arg == NULL) {
        printf("Error: failed to allocate memory for argument\n");
        palloc_free_page(original);
        free_list(&cmd_list);
        goto done;
    }
    strlcpy(arg->arg, token, strlen(token) + 1);
    // printf("%s\n", arg->arg);
    // Add the argument to the list
    list_push_back(&cmd_list, &arg->elem);
    argc++;
    token = strtok_r(NULL, " ", &save_ptr);
  }

  palloc_free_page(original);

  //for debug but i don't know how to debug..
  struct list_elem *e;
  int arg_num = 0;

  for (e = list_begin(&cmd_list); e != list_end(&cmd_list); e = list_next(e)) {
    arg = list_entry(e, struct arg_item, elem);
    // printf("(Debug) Argument %d: %s\n", arg_num, arg->arg);  // Print the argument
    arg_num++;
  }

  /* Open executable file. */
  //first_element's data of list
  // struct list_elem *e;
  e = list_begin(&cmd_list);
  struct arg_item *program_name = list_entry(e, struct arg_item, elem);
  file = filesys_open(program_name->arg);
  if (file == NULL) 
  {
    printf ("load: %s: open failed\n", program_name->arg);
    free_list(&cmd_list);
    goto done;  
  }
  // printf("after file_open\n");
  /* Read and verify executable header. */
  if (file_read (file, &ehdr, sizeof ehdr) != sizeof ehdr
      || memcmp (ehdr.e_ident, "\177ELF\1\1\1", 7)
      || ehdr.e_type != 2
      || ehdr.e_machine != 3
      || ehdr.e_version != 1
      || ehdr.e_phentsize != sizeof (struct Elf32_Phdr)
      || ehdr.e_phnum > 1024) 
    {
      printf ("load: %s: error loading executable\n", program_name->arg);
      free_list(&cmd_list);
      goto done; 
    }

  /* Read program headers. */
  file_ofs = ehdr.e_phoff;
  for (i = 0; i < ehdr.e_phnum; i++) 
    {
      struct Elf32_Phdr phdr;

      if (file_ofs < 0 || file_ofs > file_length (file))
        goto done;
      file_seek (file, file_ofs);

      if (file_read (file, &phdr, sizeof phdr) != sizeof phdr)
        goto done;
      file_ofs += sizeof phdr;
      switch (phdr.p_type) 
        {
        case PT_NULL:
        case PT_NOTE:
        case PT_PHDR:
        case PT_STACK:
        default:
          /* Ignore this segment. */
          break;
        case PT_DYNAMIC:
        case PT_INTERP:
        case PT_SHLIB:
          goto done;
        case PT_LOAD:
          if (validate_segment (&phdr, file)) 
            {
              bool writable = (phdr.p_flags & PF_W) != 0;
              uint32_t file_page = phdr.p_offset & ~PGMASK;
              uint32_t mem_page = phdr.p_vaddr & ~PGMASK;
              uint32_t page_offset = phdr.p_vaddr & PGMASK;
              uint32_t read_bytes, zero_bytes;
              if (phdr.p_filesz > 0)
                {
                  /* Normal segment.
                     Read initial part from disk and zero the rest. */
                  read_bytes = page_offset + phdr.p_filesz;
                  zero_bytes = (ROUND_UP (page_offset + phdr.p_memsz, PGSIZE)
                                - read_bytes);
                }
              else 
                {
                  /* Entirely zero.
                     Don't read anything from disk. */
                  read_bytes = 0;
                  zero_bytes = ROUND_UP (page_offset + phdr.p_memsz, PGSIZE);
                }
              if (!load_segment (file, file_page, (void *) mem_page,
                                 read_bytes, zero_bytes, writable))
                goto done;
            }
          else
            goto done;
          break;
        }
    }
  // printf("before set_upstack\n");
  /* Set up stack. */
  if (!setup_stack (esp)){
    free_list(&cmd_list);
    goto done;
  }
  // printf("after set_upstack\n");

  //TODO: construct stack
  
  //to push user stack, reverse the agrument list
  list_reverse(&cmd_list);
  // char **arg_address = malloc(sizeof(char*) *argc); //for argument address
  char **arg_address = palloc_get_page(0); //for argument address
  if(arg_address == NULL){
    printf("Error: failed to allocate memory for arg_address\n");
    free_list(&cmd_list);
    goto done;
  }
  int idx = argc - 1;
  // printf("before push argument\n");
  //push arguments in stack
  for(e = list_begin(&cmd_list); e != list_end(&cmd_list); e = list_next(e)){
    struct arg_item *arg_item = list_entry(e, struct arg_item, elem);
    *esp -= strlen(arg_item->arg) + 1;
    memcpy(*esp, arg_item->arg, strlen(arg_item->arg) + 1);
    arg_address[idx] = *esp;
    idx -= 1;
  }
  // printf("after push argument\n");

  //word-align for 80x86
  while ((uintptr_t) *esp % 4 != 0) 
  {
    *esp -= 1;
    **(uint8_t **)esp = 0;  //adding padding one byte
  }

  //push argument's address in stack
  *esp -= sizeof(char*);
  *(char*) *esp = 0;
  for(idx = argc - 1; idx >= 0; idx-=1){
    *esp -= sizeof(char *);
    *(char**)*esp = arg_address[idx];
  }
  //push argv's start's adress
  char **argv_ptr = *esp;
  *esp -= sizeof(char **);
  *(char ***) *esp = argv_ptr;
  //push argc
  *esp -= sizeof(int);
  *(int*) *esp = argc;


  //push return address
  *esp -= sizeof(void *);
  *(void **) *esp = 0;

  free_list(&cmd_list);
  palloc_free_page(arg_address);

  /* Start address. */
  *eip = (void (*) (void)) ehdr.e_entry;
  
  success = true;
 done:
  /* We arrive here whether the load is successful or not. */
  file_close (file);
  return success;
}


void free_list(struct list *list){
    while (!list_empty(list)) {
      // Get the first element in the list
      struct list_elem *e = list_pop_front(list);
      // Get the corresponding arg_item from the list element
      struct arg_item *arg = list_entry(e, struct arg_item, elem);

      // Free the dynamically allocated argument string
      // palloc_free_page(arg->arg);
      // Free the dynamically allocated arg_item
      palloc_free_page(arg);
  }
}
/* load() helpers. */

static bool install_page (void *upage, void *kpage, bool writable);

/* Checks whether PHDR describes a valid, loadable segment in
   FILE and returns true if so, false otherwise. */
static bool
validate_segment (const struct Elf32_Phdr *phdr, struct file *file) 
{
  /* p_offset and p_vaddr must have the same page offset. */
  if ((phdr->p_offset & PGMASK) != (phdr->p_vaddr & PGMASK)) 
    return false; 

  /* p_offset must point within FILE. */
  if (phdr->p_offset > (Elf32_Off) file_length (file)) 
    return false;

  /* p_memsz must be at least as big as p_filesz. */
  if (phdr->p_memsz < phdr->p_filesz) 
    return false; 

  /* The segment must not be empty. */
  if (phdr->p_memsz == 0)
    return false;
  
  /* The virtual memory region must both start and end within the
     user address space range. */
  if (!is_user_vaddr ((void *) phdr->p_vaddr))
    return false;
  if (!is_user_vaddr ((void *) (phdr->p_vaddr + phdr->p_memsz)))
    return false;

  /* The region cannot "wrap around" across the kernel virtual
     address space. */
  if (phdr->p_vaddr + phdr->p_memsz < phdr->p_vaddr)
    return false;

  /* Disallow mapping page 0.
     Not only is it a bad idea to map page 0, but if we allowed
     it then user code that passed a null pointer to system calls
     could quite likely panic the kernel by way of null pointer
     assertions in memcpy(), etc. */
  if (phdr->p_vaddr < PGSIZE)
    return false;

  /* It's okay. */
  return true;
}

/* Loads a segment starting at offset OFS in FILE at address
   UPAGE.  In total, READ_BYTES + ZERO_BYTES bytes of virtual
   memory are initialized, as follows:

        - READ_BYTES bytes at UPAGE must be read from FILE
          starting at offset OFS.

        - ZERO_BYTES bytes at UPAGE + READ_BYTES must be zeroed.

   The pages initialized by this function must be writable by the
   user process if WRITABLE is true, read-only otherwise.

   Return true if successful, false if a memory allocation error
   or disk read error occurs. */
static bool
load_segment (struct file *file, off_t ofs, uint8_t *upage,
              uint32_t read_bytes, uint32_t zero_bytes, bool writable) 
{
  ASSERT ((read_bytes + zero_bytes) % PGSIZE == 0);
  ASSERT (pg_ofs (upage) == 0);
  ASSERT (ofs % PGSIZE == 0);

  file_seek (file, ofs);
  while (read_bytes > 0 || zero_bytes > 0) 
    {
      /* Calculate how to fill this page.
         We will read PAGE_READ_BYTES bytes from FILE
         and zero the final PAGE_ZERO_BYTES bytes. */
      size_t page_read_bytes = read_bytes < PGSIZE ? read_bytes : PGSIZE;
      size_t page_zero_bytes = PGSIZE - page_read_bytes;

      /* Get a page of memory. */
      uint8_t *kpage = palloc_get_page (PAL_USER);
      if (kpage == NULL)
        return false;

      /* Load this page. */
      if (file_read (file, kpage, page_read_bytes) != (int) page_read_bytes)
        {
          palloc_free_page (kpage);
          return false; 
        }
      memset (kpage + page_read_bytes, 0, page_zero_bytes);

      /* Add the page to the process's address space. */
      if (!install_page (upage, kpage, writable)) 
        {
          palloc_free_page (kpage);
          return false; 
        }

      /* Advance. */
      read_bytes -= page_read_bytes;
      zero_bytes -= page_zero_bytes;
      upage += PGSIZE;
    }
  return true;
}

/* Create a minimal stack by mapping a zeroed page at the top of
   user virtual memory. */
static bool
setup_stack (void **esp) 
{
  uint8_t *kpage;
  bool success = false;

  kpage = palloc_get_page (PAL_USER | PAL_ZERO);
  if (kpage != NULL) 
    {
      success = install_page (((uint8_t *) PHYS_BASE) - PGSIZE, kpage, true);
      if (success)
        *esp = PHYS_BASE;
      else
        palloc_free_page (kpage);
    }
  return success;
}

/* Adds a mapping from user virtual address UPAGE to kernel
   virtual address KPAGE to the page table.
   If WRITABLE is true, the user process may modify the page;
   otherwise, it is read-only.
   UPAGE must not already be mapped.
   KPAGE should probably be a page obtained from the user pool
   with palloc_get_page().
   Returns true on success, false if UPAGE is already mapped or
   if memory allocation fails. */
static bool
install_page (void *upage, void *kpage, bool writable)
{
  struct thread *t = thread_current ();

  /* Verify that there's not already a page at that virtual
     address, then map our page there. */
  return (pagedir_get_page (t->pagedir, upage) == NULL
          && pagedir_set_page (t->pagedir, upage, kpage, writable));
}
