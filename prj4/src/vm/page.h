#ifndef VM_PAGE_H
#define VM_PAGE_H

#include <hash.h>
#include "filesys/file.h"
#include "threads/thread.h"

enum page_status{
    LOAD,  // memory에 load되어 있는 상태
    SWAP,  // swap disk 에 있는 상태
    FILE, //file에서만 있는 상태
    ZERO,   
};


/* Supplemental Page Table Entry */
struct spt_entry{

    void *upage;            // User virtual page address
    
    enum page_status status; // Page status

    bool writable;          // Writable or not

    struct file *file;      // File pointer
    off_t offset;           // Offset

    size_t read_bytes;      // Read bytes
    size_t zero_bytes;      // Zero bytes
    
    size_t swap_slot;      // Swap index

    struct hash_elem elem;  // Hash element
};

/* Functions to manage the supplemental page table */
bool spt_insert(struct spt_entry *spte);
void spt_destroy(struct hash *spt);
struct spt_entry *spt_find(void *upage);
void spt_free_entry(struct hash_elem *e, void *aux);


#endif