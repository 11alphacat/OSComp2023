#include "common.h"
#include "list.h"
#include "memory/mm.h"
#include "memory/allocator.h"
#include "debug.h"
#include "proc/proc_mm.h"
#include "memory/vma.h"

/* allocate a mm_struct, with root pagetable and head_vma */
struct mm_struct *alloc_mm() {
    struct mm_struct *mm;
    mm = kzalloc(sizeof(struct mm_struct));
    if (mm == NULL) {
        Warn("alloc_mm: no free memory");
        return NULL;
    }

    INIT_LIST_HEAD(&mm->head_vma);

    // an empty user page table.
    mm->pagetable = proc_pagetable();
    if (mm->pagetable == 0) {
        goto bad;
    }
    return mm;

bad:
    kfree(mm);
    return NULL;
}

/* TODO: optimize interface argument */
void free_mm(struct mm_struct *mm, int thread_cnt) {
    if (mm == NULL) {
        Warn("no need to free mm");
        return;
    }

    if (mm->pagetable)
        proc_freepagetable(mm, thread_cnt);

    /* we use vma in proc_freepagetable, so free it after that */
    free_all_vmas(mm);
    mm->pagetable = 0;
    mm->brk = 0;
    kfree(mm);
}