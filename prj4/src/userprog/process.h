#ifndef USERPROG_PROCESS_H
#define USERPROG_PROCESS_H

#include "threads/thread.h"
#include <stdbool.h>   
#include <string.h>

//for list item
struct arg_item{
    char arg[128];
    struct list_elem elem;  // List element for linking
};

tid_t process_execute (const char *file_name);
int process_wait (tid_t);
void process_exit (void);
void process_activate (void);
void free_list(struct list *list);
bool make_file_page(void *upage, void *kpage, bool writable);

#endif /* userprog/process.h */
