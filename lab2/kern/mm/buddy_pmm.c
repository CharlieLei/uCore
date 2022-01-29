//
// Created by leich on 2022/1/29.
//
#include <pmm.h>
#include <string.h>
#include <buddy_pmm.h>

struct buddy_control_block control_block;

static void
buddy_init(void) {

}

static void
buddy_init_memmap(struct Page *base, size_t n) {
    assert(n > 0);
    int i, max_page_num = 1;
    // 伙伴算法要求页的个数是2的幂，空闲的物理内存中的个数可能不是2的幂
    // 控制块也要占据内存
    while (max_page_num + (max_page_num >> BLOCK_SIZE_SHIFT) <= n)
        max_page_num *= 2;
    max_page_num /= 2;

    control_block.block_page_num = (max_page_num >> BLOCK_SIZE_SHIFT) + 1;
    control_block.max_allocatable_page_num = max_page_num;
    control_block.allocatable_page_base = base + control_block.block_page_num;
    cprintf("buddy init:: page num: total %d, block use %d, allocatable %d\n",
            n, control_block.block_page_num, control_block.max_allocatable_page_num);

    // 将控制块所用的页设为不可分配
    struct Page *p;
    for(p = base; p != base + control_block.block_page_num; ++p)
        SetPageReserved(p);
    // 将可分配的页进行初始化
    struct Page *allocatable_base = control_block.allocatable_page_base;
    for(p = allocatable_base; p != allocatable_base + control_block.max_allocatable_page_num; ++p) {
        assert(PageReserved(p));
        p->flags = p->property = 0;
        ClearPageReserved(p);
        SetPageProperty(p);
        set_page_ref(p, 0);
    }
    // 初始化控制块
    // 为控制块中的数组分配空间
    control_block.alloc_tree = (uintptr_t *)KADDR(page2pa(base));
    // 将二叉树的叶节点全部设为1
    for (i = max_page_num; i < max_page_num * 2; ++i)
        control_block.alloc_tree[i] = 1;
    // 设置二叉树的内部节点
    for (i = max_page_num - 1; i > 0; --i)
        control_block.alloc_tree[i] = control_block.alloc_tree[LEFT_CHILD(i)] * 2;
}

static struct Page *
buddy_alloc_pages(size_t n) {
    assert(n > 0);
    if (n > control_block.alloc_tree[ROOT])
         return NULL;

    int node_size = control_block.max_allocatable_page_num, index = 1;
    for (; node_size > n; node_size /= 2) {
        if (control_block.alloc_tree[LEFT_CHILD(index)] >= n) {
            // 往左子树走
            index = LEFT_CHILD(index);
        } else {
            // 往右子树走
            index = RIGHT_CHILD(index);
        }
    }

    control_block.alloc_tree[index] = 0;
    // 将分配的页设置为可用
    struct Page *page = control_block.allocatable_page_base + index * node_size - control_block.max_allocatable_page_num;
    struct Page *p;
    for (p = page; p != page + n; ++p) {
//        ClearPageReserved(p);
        ClearPageProperty(p);
        set_page_ref(p, 0);
    }
    // 从下往上更新二叉树的值
    index /= 2;
    for (; index >= ROOT; index = PARENT(index)) {
        control_block.alloc_tree[index] = MAX(control_block.alloc_tree[LEFT_CHILD(index)]
                                              ,control_block.alloc_tree[RIGHT_CHILD(index)]);
    }

    return page;
}

static void
buddy_free_pages(struct Page *base, size_t n) {
    assert(n > 0);
    // 从叶节点往上，找第一个节点值为0的结点
    unsigned int index = (unsigned int)(base - control_block.allocatable_page_base) + control_block.max_allocatable_page_num;
    unsigned int node_size = 1;
    for (; control_block.alloc_tree[index] > 0 && index > 1; index = PARENT(index)) {
        node_size *= 2;
    }

    // 释放所有页
    struct Page *p;
    for (p = base; p != base + n; ++p) {
        assert(!PageReserved(p) && !PageProperty(p));
        p->flags = 0;
        SetPageProperty(p);
        set_page_ref(p, 0);
    }

    control_block.alloc_tree[index] = node_size;
    unsigned int left_val, right_val;
    // 从下往上修改二叉树的值
    for (index = PARENT(index); index > ROOT; index = PARENT(index)) {
        node_size *= 2;
        left_val = control_block.alloc_tree[LEFT_CHILD(index)];
        right_val = control_block.alloc_tree[RIGHT_CHILD(index)];
        if (left_val + right_val == node_size) {
            control_block.alloc_tree[index] = node_size;
        } else {
            control_block.alloc_tree[index] = MAX(left_val, right_val);
        }
    }
}

static size_t
buddy_nr_free_pages(void) {
    return control_block.alloc_tree[1];
}

static void
buddy_check(void) {
    int all_pages = nr_free_pages();
    struct Page* p0, *p1, *p2, *p3;
    assert(alloc_pages(all_pages + 1) == NULL);

    p0 = alloc_pages(1);
    assert(p0 != NULL);
    p1 = alloc_pages(2);
    assert(p1 == p0 + 2);
    assert(!PageReserved(p0) && !PageProperty(p0));
    assert(!PageReserved(p1) && !PageProperty(p1));

    p2 = alloc_pages(1);
    assert(p2 == p0 + 1);
    p3 = alloc_pages(2);
    assert(p3 == p0 + 4);
    assert(!PageProperty(p3) && !PageProperty(p3 + 1) && PageProperty(p3 + 2));

    free_pages(p1, 2);
    assert(PageProperty(p1) && PageProperty(p1 + 1));
    assert(p1->ref == 0);

    free_pages(p0, 1);
    free_pages(p2, 1);

    p2 = alloc_pages(2);
    assert(p2 == p0);
    free_pages(p2, 2);
    assert((*(p2 + 1)).ref == 0);
    assert(nr_free_pages() == all_pages / 2);

    free_pages(p3, 2);
    p1 = alloc_pages(129);
    free_pages(p1, 129);
}

const struct pmm_manager buddy_pmm_manager = {
    .name = "buddy_pmm_manager",
    .init = buddy_init,
    .init_memmap = buddy_init_memmap,
    .alloc_pages = buddy_alloc_pages,
    .free_pages = buddy_free_pages,
    .nr_free_pages = buddy_nr_free_pages,
    .check = buddy_check,
};