#include "vm/page.h"
#include "vm/frame.h"
#include <bitmap.h>
#include "devices/block.h"

struct swap_table{
    struct bitmap *swap_bitmap;
    struct block *swap_disk;
};


void swap_table_init(void);
void swap_in(size_t slot_index, void *page);
size_t swap_out(void *page);