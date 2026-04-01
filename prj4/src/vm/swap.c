#include "vm/swap.h"
#include "threads/vaddr.h"
#include "threads/malloc.h"
#include "devices/block.h"
#include "threads/synch.h"
#include <bitmap.h>

static struct swap_table swap_table;
static struct lock swap_lock;

void swap_table_init(void) {
    swap_table.swap_disk = block_get_role(BLOCK_SWAP);
    if (!swap_table.swap_disk) {
        return;
    }
    size_t swap_size = block_size(swap_table.swap_disk) / (PGSIZE / BLOCK_SECTOR_SIZE);
    swap_table.swap_bitmap = bitmap_create(swap_size);
    if (!swap_table.swap_bitmap) {
        printf("Failed to create swap bitmap!\n");
        // PANIC("Failed to create swap bitmap!");
    }
    lock_init(&swap_lock);
    bitmap_set_all(swap_table.swap_bitmap, false); // 초기화
}

void swap_in(size_t slot_index, void *page) {
    // lock_acquire(&swap_lock);
    if (!bitmap_test(swap_table.swap_bitmap, slot_index)) {
        printf("Invalid swap slot access!\n");
        // PANIC("Invalid swap slot access!");
        lock_release(&swap_lock);
    }
    for (size_t i = 0; i < PGSIZE / BLOCK_SECTOR_SIZE; i++) {
        block_read(swap_table.swap_disk, slot_index * (PGSIZE / BLOCK_SECTOR_SIZE) + i,
                   (uint8_t *)page + i * BLOCK_SECTOR_SIZE);
    }
    bitmap_flip(swap_table.swap_bitmap, slot_index); // 슬롯 해제
    // lock_release(&swap_lock);
}

size_t swap_out(void *page) {
    // lock_acquire(&swap_lock);
    size_t free_slot = bitmap_scan_and_flip(swap_table.swap_bitmap, 0, 1, false);
    if (free_slot == BITMAP_ERROR) {
        printf("No free swap slots!\n");
        // PANIC("No free swap slots!");
        lock_release(&swap_lock);
    }
    for (size_t i = 0; i < PGSIZE / BLOCK_SECTOR_SIZE; i++) {
        block_write(swap_table.swap_disk, free_slot * (PGSIZE / BLOCK_SECTOR_SIZE) + i,
                    (uint8_t *)page + i * BLOCK_SECTOR_SIZE);
    }
    // lock_release(&swap_lock);
    return free_slot;
}