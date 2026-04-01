#include "frame.h"
#include "threads/palloc.h"
#include "threads/malloc.h"
#include "threads/vaddr.h"
#include "userprog/pagedir.h"
#include <list.h>
#include <debug.h>
#include <stdbool.h>
#include <stdio.h>
#include  "swap.h"

/* Frame Table */
static struct list frame_table;
static struct lock frame_table_lock;
/* Initialize the frame table */
void frame_table_init(void) {
    list_init(&frame_table);
    lock_init(&frame_table_lock);
    // printf("Frame table and lock initialized.\n");
}

/* Allocate a frame for the given page */
void *frame_alloc(uint8_t *page, bool zero) {
    void *frame;
    // printf("Thread %s trying to acquire lock\n", thread_current()->name);
    lock_acquire(&frame_table_lock);
    // printf("Thread %s acquired lock\n", thread_current()->name);
    // printf("Frame alloc\n");
    /* Allocate a physical frame */
    ASSERT(page != NULL);

    frame = zero ? palloc_get_page(PAL_USER | PAL_ZERO) : palloc_get_page(PAL_USER);
    if(frame == NULL){  
        // printf("Frame alloc failed\n");
        frame_evict();
        // printf("frame evict success\n");
        frame = zero ? palloc_get_page(PAL_USER | PAL_ZERO) : palloc_get_page(PAL_USER);
        if (frame == NULL) {
            printf("Frame allocation failed!\n");
            lock_release(&frame_table_lock);
            return NULL;
        }
    }
    /* Create and initialize a new frame table entry */
    struct frame_table_entry *fte = malloc(sizeof(struct frame_table_entry));
    // frame table entry가 할당되지 않은 경우
    if (fte == NULL) {
        printf("Frame table entry allocation failed!\n");
        palloc_free_page(frame);
        lock_release(&frame_table_lock);
        return NULL;
    }
    fte->frame = frame;
    fte->page = page;
    fte->owner = thread_current();
    fte->accessed = true;
    fte->pinned = false;
    list_push_back(&frame_table, &fte->elem);
    lock_release(&frame_table_lock);
    return frame;
}

/* Free the given frame */
void frame_free(struct frame_table_entry *fte) {
    list_remove(&fte->elem);
    pagedir_clear_page(fte->owner->pagedir, fte->page);
    palloc_free_page(fte->frame);
    free(fte);
}

void frame_evict(void) {
    if(list_empty(&frame_table)){
        return;
    }
    static struct list_elem *victim = NULL; // 원형 큐의 "현재 위치"
    if (victim == NULL || victim == list_end(&frame_table)) {
        victim = list_begin(&frame_table); // 첫 번째 프레임에서 시작
    }
    struct frame_table_entry *fte;
    while (true) {
        // printf("Frame evict while\n");
        fte = list_entry(victim, struct frame_table_entry, elem);
        if(fte == NULL){
            return;
        }
        if (fte->pinned) {
            victim = list_next(victim);
            if (victim == list_end(&frame_table)) {
                victim = list_begin(&frame_table);
            }
            continue;
        }
        // 참조되지 않은 프레임을 찾음
        if (!(fte->accessed)) {
            struct spt_entry *spte = spt_find(fte->page);  // SPT에서 해당 페이지 찾기
            if (spte == NULL) {
                printf("SPT entry not found for evicted page!\n");
                return;
            }
            // Swap Out 처리
            size_t swap_slot = swap_out(fte->page);
            spte->status = SWAP;         // SPT 상태를 SWAP으로 변경
            spte->swap_slot = swap_slot; // 스왑 슬롯 정보 저장
            victim = list_remove(victim);
            frame_free(fte);      // 프레임 해제
            return;
        } else {
            fte->accessed = false;
            victim = list_next(victim);
            if (victim == list_end(&frame_table)) {
                victim = list_begin(&frame_table); // 원형큐
            }
        }
    }
}
