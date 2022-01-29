//
// Created by leich on 2022/1/29.
//

#ifndef __KERN_MM_BUDDY_PMM_H__
#define __KERN_MM_BUDDY_PMM_H__

#include <pmm.h>

// 控制块每一项需要4B，若分配nB内存，则需要2n * 4B = 8nB内存存整个控制块
// 因此控制块需要 8n/4096 个页
#define BLOCK_SIZE_SHIFT 9

#define ROOT 1
#define LEFT_CHILD(index) (index * 2)
#define RIGHT_CHILD(index) (index * 2 + 1)
#define PARENT(index) (index / 2)

#define MAX(a,b) ((a > b)? (a): (b))

extern const struct pmm_manager buddy_pmm_manager;

struct buddy_control_block {
    uint32_t block_page_num;
    uint32_t max_allocatable_page_num;
    uintptr_t *alloc_tree;
    struct Page *allocatable_page_base;
};

#endif //__KERN_MM_BUDDY_PMM_H__
