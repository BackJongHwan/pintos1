#include "userprog/syscall.h"
#include <stdio.h>
#include <syscall-nr.h>
#include "threads/interrupt.h"
#include "threads/thread.h"
//my
#include "userprog/process.h"
#include "threads/vaddr.h"
#include "devices/shutdown.h"  // shutdown_power_off 함수 선언을 위한 헤더 파일
#include "devices/input.h" //for input_getc()
#include "userprog/pagedir.h"

static void syscall_handler (struct intr_frame *);

void
syscall_init (void) 
{
  intr_register_int (0x30, 3, INTR_ON, syscall_handler, "syscall");
}

//TODO: syscall_handler implement
static void
syscall_handler (struct intr_frame *f UNUSED) 
{
  //implement exit, halt, exec, wait
  //read, write standard input/output!!
  //exit -> shutdown_power_off()
  //exit -> terminate current user program, returning status to the kernel
  //exec() -> 1. create child process
  // 2. refer to process_execute() in userprog/proceess.c
  //wait() -> 

  //valid
  if(!is_valid_user_pointer(f->esp)){
    Exit(-1);
  } 
  int syscall_num = *(int*)f->esp;
  switch (syscall_num)
  {
  case SYS_HALT:
    Halt();
    break;
  case SYS_EXIT:
    if(!is_valid_user_pointer(f->esp + 4)){
      Exit(-1);
    }
    int status = *((int *)(f->esp + 4));
    Exit(status);
    break;
  case SYS_EXEC:
    if(!is_valid_user_pointer(f->esp + 4)){
      Exit(-1);
    }
    f->eax = Exec(*(const char**)(f->esp + 4));
    break;
  case SYS_WAIT:
    if (!is_valid_user_pointer(f->esp + 4)) {
        Exit(-1);  
    }
    tid_t tid = *(tid_t *)(f->esp + 4);
    f->eax = Wait(tid);
    break;
  case SYS_WRITE:
    if(!is_valid_user_pointer(f->esp + 4) || !is_valid_user_pointer(f->esp + 8) || !is_valid_user_pointer(f->esp + 12) ){
      Exit(-1);
    }
    f->eax = Write(*(int*)(f->esp + 4), *(void**)(f->esp + 8), *(unsigned *)(f->esp + 12));
    break;
  case SYS_READ:
    if(!is_valid_user_pointer(f->esp + 4) || !is_valid_user_pointer(f->esp + 8) || !is_valid_user_pointer(f->esp + 12) ){
      Exit(-1);
    }
    f->eax = Read(*(int*)(f->esp + 4), *(void**)(f->esp + 8), *(unsigned *)(f->esp + 12));
    break;
  case SYS_FIBO:
    if(!is_valid_user_pointer(f->esp + 4)){
      Exit(-1);
    }
    f->eax = Fibonacci(*(int*)(f->esp + 4));
    break;
  case SYS_MAX_FOUR_INT:
    if(!is_valid_user_pointer(f->esp + 4) ||!is_valid_user_pointer(f->esp + 8) ||!is_valid_user_pointer(f->esp + 12) | !is_valid_user_pointer(f->esp + 16) ){
      Exit(-1);
    }
    f->eax = Max_of_four_int(*(int*)(f->esp + 4),*(int*)(f->esp + 8), *(int*)(f->esp + 12), *(int*)(f->esp + 16) );
    break;
  default:
    printf("unknown system call!!\n");
    Exit(-1);
  }
}
int is_valid_user_pointer(const void * uaddr){
    if (uaddr == NULL) {
        return false;
    }
    //is user_vadder
    if (!is_user_vaddr(uaddr)) {
        return false;
    }
    void *ptr = pagedir_get_page(thread_current()->pagedir, uaddr);
    if(ptr == NULL){
      return false;
    }

    return true;
}
/*shut down system*/
void Halt(void){
  shutdown_power_off();
}

/*Terminate current thread*/
void Exit(int status)
{
  printf("%s: exit(%d)\n", thread_name(), status);
  thread_current()->exit_status = status;
  thread_exit();
}

tid_t Exec(const char* file){
  // printf("Exec called with file_name: %s\n", file);  // file 값 확인
  if (!is_valid_user_pointer(file)) {
      Exit(-1);  
  }
  return process_execute(file);
}

int Wait(tid_t pid){
  return process_wait(pid);
}

int Write(int fd, void *buffer, unsigned size){
  if(fd != 1){
    Exit(-1);
  }
  putbuf(buffer, size);
  return size;
}

int Read(int fd, void *buffer, unsigned size){
  if(fd != 0){
    Exit(-1);
  }
  for(unsigned i = 0; i < size; i++){
    *((char *)buffer + i) = input_getc();
  }
  return size;
}

int Fibonacci(int n){
  if(n < 0){
    printf("invalid input argument!\n");
    Exit(-1);
  }
  if(n == 0){
    return 0;
  }
  if(n == 1){
    return 1;
  }
  int fn_first = 0;
  int fn_second = 1;
  int result;
  for(int i = 2; i <= n; i++){
    result = fn_first + fn_second;
    fn_first= fn_second;
    fn_second = result; 
  }
  return result;
}



int Max_of_four_int(int a, int b, int c, int d){
  int max = a;
  if(b > max) max = b;
  if(c > max) max = c;
  if(d > max) max = d;
  return max;
}