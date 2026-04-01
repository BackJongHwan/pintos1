#ifndef USERPROG_SYSCALL_H
#define USERPROG_SYSCALL_H

typedef int tid_t;

void syscall_init (void);

void Halt(void);
void Exit(int status);
tid_t Exec(const char* file);
int Wait(tid_t pid);
int Write(int fd, void *buffer, unsigned size);
int Read(int fd, void *buffer, unsigned size);
int is_valid_user_pointer(const void *uaddr);
int Fibonacci(int n);
int Max_of_four_int(int a, int b, int c, int d);
#endif /* userprog/syscall.h */
