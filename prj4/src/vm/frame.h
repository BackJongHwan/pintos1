#ifndef VM_FRAME_H
#define VM_FRAME_H

#include <list.h>
#include <stdbool.h>
#include <bitmap.h>
#include "threads/synch.h"
#include "threads/thread.h"
#include "threads/palloc.h"

/* Frame Table Entry Structure */
struct frame_table_entry {
    uint8_t *frame;              // Physical frame address
    uint8_t *page;               // Virtual page address
    struct list_elem elem;    // Element for list
    struct thread *owner;     // Thread that owns the frame
    bool accessed;            // Accessed bit 
    bool pinned;
};

/* Functions to manage the frame table */
void frame_table_init(void);
void *frame_alloc(uint8_t *page, bool zero);
void frame_free(struct frame_table_entry *fte);
void frame_evict(void);

#endif /* VM_FRAME_H */
