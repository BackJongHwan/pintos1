#ifndef USERPROG_SYSCALL_H
#define USERPROG_SYSCALL_H
#include<stdbool.h>

typedef int tid_t;

void syscall_init (void);
//project1
void halt(void);
void exit(int status);
tid_t exec(const char* file);
int wait(tid_t pid);
int write(int fd, void *buffer, unsigned size);
int read(int fd, void *buffer, unsigned size);
int is_valid_user_pointer(const void *uaddr);
int Fibonacci(int n);
int Max_of_four_int(int a, int b, int c, int d);

//project2
int open (const char *file);
bool create (const char *file, unsigned initial_size);
bool remove (const char *file);
int filesize (int fd);
void seek (int fd, unsigned position);
unsigned tell (int fd);
void close (int fd);


#endif /* userprog/syscall.h */
