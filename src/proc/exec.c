#include "common.h"
#include "param.h"
#include "memory/memlayout.h"
#include "lib/riscv.h"
#include "atomic/spinlock.h"
#include "proc/pcb_life.h"
#include "proc/exec.h"
#include "debug.h"
#include "memory/vm.h"
#include "kernel/trap.h"
#include "proc/pcb_mm.h"
#include "fs/vfs/ops.h"
#include "fs/fat/fat32_file.h"
#include "fs/fat/fat32_mem.h"
#include "proc/tcb_life.h"
#include "lib/auxv.h"
#include "lib/ctype.h"
#include "memory/vma.h"

void print_ustack(pagetable_t pagetable, uint64 stacktop);
/* this will commit to trapframe after execve success */
struct commit {
    uint64 entry; /* epc */
    uint64 a1;
    uint64 a2;
    uint64 sp;
};

#define AUX_CNT 38
typedef struct {
    int a_type;
    union {
        long a_val;
        void *a_ptr;
        void (*a_fnc)();
    } a_un;
} auxv_t;

int do_execve(char *path, char *const argv[], char *const envp[]);

int flags2perm(int flags) {
    int perm = 0;
    if (flags & 0x1)
        perm = PTE_X;
    if (flags & 0x2)
        perm |= PTE_W;
    if (flags & 0x4)
        perm |= PTE_R;
    return perm;
}

int flags2vmaperm(int flags) {
    int perm = 0;
    if (flags & 0x1)
        perm = PERM_EXEC;
    if (flags & 0x2)
        perm |= PERM_WRITE;
    if (flags & 0x4)
        perm |= PERM_READ;
    return perm;
}

// Load a program segment into pagetable at virtual address va.
// va must be page-aligned
// and the pages from va to va+sz must already be mapped.
// Returns 0 on success, -1 on failure.
static int
loadseg(pagetable_t pagetable, uint64 va, struct inode *ip, uint offset, uint sz) {
    uint i, n;
    uint64 pa;

    for (i = 0; i < sz; i += PGSIZE) {
        pa = walkaddr(pagetable, va + i);
        if (pa == 0)
            panic("loadseg: address should exist");
        if (sz - i < PGSIZE)
            n = sz - i;
        else
            n = PGSIZE;
        if (fat32_inode_read(ip, 0, (uint64)pa, offset + i, n) != n)
            return -1;
    }

    return 0;
}

vaddr_t p_argc, p_envp, p_argv, p_auxv;
/* return argc, or -1 to indicate error */
static int ustack_init(struct proc *p, pagetable_t pagetable, struct commit *commit, vaddr_t sp, char *const argv[], char *const envp[]) {
    /* sp has to be the top address of a page */
    ASSERT(sp % PGSIZE == 0);
    vaddr_t stackbase;
    stackbase = sp - USTACK_PAGE * PGSIZE;

    /*       Initial Process Stack (follows SYSTEM V ABI)
        +---------------------------+ <-- High Address
        |     Information block     |
        +---------------------------+
        |         Unspecified       |
        +---------------------------+
        |    Null aux vector entry  |
        +---------------------------+
        |  Auxiliary vector entries |
        +---------------------------+
        |            0              |
        +---------------------------+
        |           ...             |
        |        envp pointers      |
        |           ...             |
        +---------------------------+
        |            0              |
        +---------------------------+
        |           ...             |
        |        argv pointers      |
        |           ...             |
        +---------------------------+
        |           argc            |
        +---------------------------+ <-- Low Address
        |           ...             |
        +---------------------------+
    */

    uint64 argc = 0, envpc = 0;
    vaddr_t argv_addr[MAXARG], envp_addr[MAXENV];
    /* Information block */
    if (argv != 0) {
        for (; argv[argc]; argc++) {
            if (argc >= MAXARG)
                return -1;
            paddr_t cp = getphyaddr(p->mm->pagetable, (vaddr_t)argv[argc]);
            if (cp == 0) {
                return -1;
            }
            sp -= strlen((const char *)cp) + 1;
            sp -= sp % 8;
            if (sp < stackbase)
                return -1;
            if (copyout(pagetable, sp, (char *)cp, strlen((const char *)cp) + 1) < 0)
                return -1;
            argv_addr[argc] = sp;
        }
    }
    argv_addr[argc] = 0;

    if (envp != 0) {
        for (; envp[envpc]; envpc++) {
            if (envpc >= MAXENV)
                return -1;
            paddr_t cp = getphyaddr(p->mm->pagetable, (vaddr_t)envp[envpc]);
            if (cp == 0) {
                return -1;
            }
            sp -= strlen((const char *)cp) + 1;
            sp -= sp % 8;
            if (sp < stackbase)
                return -1;
            if (copyout(pagetable, sp, (char *)cp, strlen((const char *)cp) + 1) < 0)
                return -1;
            envp_addr[envpc] = sp;
        }
    }
    envp_addr[envpc] = 0;

    ASSERT(sp % 8 == 0);
    // Log("%d %d", sp % 16, (uint64)(argc + envpc + 2 + 1) % 2);
    /* to make sp 16-bit aligned */
    if ((sp % 16 != 0 && (uint64)(argc + envpc + 1 + AUX_CNT) % 2 == 0)
        || ((sp % 16 == 0) && (uint64)(argc + envpc + 2 + 1) % 2 == 1)) {
        // Log("aligned");
        sp -= 8;
    }

    /* auxiliary vectors */
    uint64 auxv[AUX_CNT * 2] = {0};
    for (int i = 0; i < AUX_CNT; i++) {
        if (i + 1 > AT_PAGESZ) {
            break;
        }
        auxv[i * 2] = i + 1;
    }
    auxv[AT_PAGESZ * 2 - 1] = PGSIZE;
    sp -= AUX_CNT * 16;
    p_auxv = sp;
    if (sp < stackbase) {
        return -1;
    }
    if (copyout(pagetable, sp, (char *)auxv, AUX_CNT * 16) < 0) {
        return -1;
    }

    // /* pad 0 */
    // uint8 temp = 0;
    // sp -= 8;
    // if (copyout(pagetable, sp, (char *)&temp, sizeof(uint8))) {
    //     return -1;
    // }

    /* push the array of envp[] pointers */
    sp -= (envpc + 1) * sizeof(uint64);
    sp -= sp % 8;
    p_envp = sp;
    // Log("envp %p", sp);
    if (sp < stackbase)
        return -1;
    if (copyout(pagetable, sp, (char *)envp_addr, (envpc + 1) * sizeof(uint64)) < 0)
        return -1;
    commit->a2 = sp;

    // /* pad 0 */
    // sp -= 8;
    // if (copyout(pagetable, sp, (char *)&temp, sizeof(uint8))) {
    //     return -1;
    // }

    /* push the array of argv[] pointers */
    sp -= (argc + 1) * sizeof(uint64);
    sp -= sp % 8;
    p_argv = sp;
    // Log("argv %p", sp);
    if (sp < stackbase)
        return -1;
    if (copyout(pagetable, sp, (char *)argv_addr, (argc + 1) * sizeof(uint64)) < 0)
        return -1;
    commit->a1 = sp;
    sp -= 8;
    p_argc = sp;
    // Log("argc %p", sp);
    if (copyout(pagetable, sp, (char *)&argc, sizeof(uint64)) < 0) {
        return -1;
    }
    // Log("%d", argc);

    commit->sp = sp; // initial stack pointer
    // Log("sp is %p", sp);
    // print_ustack(pagetable, stackbase + USTACK_PAGE * PGSIZE);
    return argc;
}

void print_ustack(pagetable_t pagetable, uint64 stacktop) {
    char *pa = (char *)getphyaddr(pagetable, stacktop - 1) + 1;
    // Log("pa is %p", pa);
    /* just print the first 100 8bits of the ustack */
    for (int i = 8; i < 100 * 8; i += 8) {
        if (i % 16 == 0) {
            printfGreen("aligned -> ");
        } else {
            printfGreen("           ");
        }

        if (stacktop - i == p_argc || stacktop - i == p_argv || stacktop - i == p_envp || stacktop - i == p_auxv) {
            printfBlue("%#p:", stacktop - i);
        } else {
            printfGreen("%#p:", stacktop - i);
        }

        for (int j = 0; j < 8; j++) {
            char c = (char)*(paddr_t *)(pa - i + j);
            if ((int)c > 0x20 && (int)c <= 0x7e) {
                printfGreen("%c", c);
            }
        }
        printf("\t");
        for (int j = 0; j < 8; j++) {
            printf("%02x ", (uint8) * (paddr_t *)(pa - i + j));
        }
        printf("\n");
    }
}

/* return the end of the virtual address after load segments, or 0 to indicate error */
static uint64 loader(char *path, struct mm_struct *mm, struct commit *commit) {
    int i, off;
    uint64 sz = 0;
    struct elfhdr elf;
    struct proghdr ph;
    struct inode *ip;

    if ((ip = namei(path)) == 0) {
        return 0;
    }
    fat32_inode_lock(ip);

    // Check ELF header
    if (fat32_inode_read(ip, 0, (uint64)&elf, 0, sizeof(elf)) != sizeof(elf))
        goto unlock_put;

    if (elf.magic != ELF_MAGIC)
        goto unlock_put;

    // Load program into memory.
    for (i = 0, off = elf.phoff; i < elf.phnum; i++, off += sizeof(ph)) {
        if (fat32_inode_read(ip, 0, (uint64)&ph, off, sizeof(ph)) != sizeof(ph))
            goto unlock_put;
        if (ph.type != ELF_PROG_LOAD)
            continue;
        if (ph.memsz < ph.filesz)
            goto unlock_put;
        if (ph.vaddr + ph.memsz < ph.vaddr)
            goto unlock_put;

        /* off: start offset in misaligned page
           size: read size of misaligned page
        */
        uint64 off = 0, size = 0;
        vaddr_t vaddrdown = PGROUNDDOWN(ph.vaddr);
        if (ph.vaddr % PGSIZE != 0) {
            // Warn("%p", vaddrdown);
            paddr_t pa = (paddr_t)kmalloc(PGSIZE);
            if (mappages(mm->pagetable, vaddrdown, PGSIZE, pa, flags2perm(ph.flags) | PTE_U, COMMONPAGE) < 0) {
                Warn("misaligned loader mappages failed");
                kfree((void *)pa);
                goto unlock_put;
            }
            off = ph.vaddr - vaddrdown;
            size = PGROUNDUP(ph.vaddr) - ph.vaddr;
            if (fat32_inode_read(ip, 0, (uint64)pa + off, ph.off, size) != size) {
                goto unlock_put;
            }
            // Log("entry is %p", elf.entry);
        } else {
            ASSERT(vaddrdown == ph.vaddr);
        }
        uint64 sz1;
        ASSERT((ph.vaddr + size) % PGSIZE == 0);
        // Log("\nstart end:%p %p", ph.vaddr + size, ph.vaddr + ph.memsz);
        if ((sz1 = uvmalloc(mm->pagetable, ph.vaddr + size, ph.vaddr + ph.memsz, flags2perm(ph.flags))) == 0)
            goto unlock_put;
        // vmprint(mm->pagetable, 1, 0, 0, 0);
        sz = sz1;
        if (loadseg(mm->pagetable, ph.vaddr + size, ip, ph.off + size, ph.filesz - size) < 0)
            goto unlock_put;

        vaddr_t vaddrup = PGROUNDUP(ph.vaddr + ph.memsz);
        if (vma_map(mm, vaddrdown, vaddrup - vaddrdown, flags2vmaperm(ph.flags), VMA_TEXT) < 0) {
            goto unlock_put;
        }
    }
    fat32_inode_unlock_put(ip);
    ip = 0;
    commit->entry = elf.entry;
    return sz;

unlock_put:
    // proc_freepagetable(mm->pagetable, sz, 0);
    fat32_inode_unlock_put(ip);
    return 0;
}

int do_execve(char *path, char *const argv[], char *const envp[]) {
    struct commit commit;
    struct proc *p = proc_current();
    struct tcb *t = thread_current();
    vaddr_t brk; /* program_break */
    memset(&commit, 0, sizeof(commit));

    struct mm_struct *mm, *oldmm = p->mm;
    mm = alloc_mm();
    if (mm == NULL) {
        return -1;
    }

    /* Loader */
    if ((brk = loader(path, mm, &commit)) == 0) {
        goto bad;
    }

    /* Heap Initialization */
    brk = PGROUNDUP(brk);
    mm->start_brk = mm->brk = brk;
    mm->heapvma = NULL;

    /* Stack Initialization */
    if (uvm_thread_stack(mm->pagetable, 0) < 0) {
        Warn("bad");
        goto bad;
    }
    if (vma_map(mm, USTACK, USTACK_PAGE * PGSIZE, PERM_READ | PERM_WRITE, VMA_STACK) < 0) {
        goto bad;
    }

    int argc = ustack_init(p, mm->pagetable, &commit, USTACK + USTACK_PAGE * PGSIZE, argv, envp);
    if (argc < 0) {
        goto bad;
    }

    /* Save program name for debugging */
    char *s, *last;
    for (last = s = path; *s; s++)
        if (*s == '/')
            last = s + 1;
    safestrcpy(p->name, last, sizeof(p->name));

    /* Commit to the user image */
    t->trapframe->sp = commit.sp;
    t->trapframe->a1 = commit.a1;
    t->trapframe->a2 = commit.a2;
    t->trapframe->epc = commit.entry;
    // uvm_thread_trapframe(mm->pagetable, 0, (paddr_t)t->trapframe);

    /* free the old pagetable */
    free_mm(oldmm, p->tg->thread_cnt);

    /* commit new mm */
    p->mm = mm;

    /* TODO */
    thread_trapframe(t, 1);

    /* for debug, print the pagetable and vmas after exec */
    // vmprint(mm->pagetable, 1, 0, 0, 0);
    // print_vma(mm);

    return argc; // this ends up in a0, the first argument to main(argc, argv)

bad:
    // TODO
    // Note: cnt is 1!
    free_mm(mm, 0);
    return -1;
}