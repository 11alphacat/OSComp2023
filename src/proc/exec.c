#include "common.h"
#include "param.h"
#include "memory/memlayout.h"
#include "riscv.h"
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
#include "proc/pcb_thread.h"

void print_ustack(pagetable_t pagetable, uint64 stacktop);
/* this will commit to trapframe after execve success */
struct commit {
    uint64 entry; /* epc */
    uint64 a1;
    uint64 a2;
    uint64 sp;
};

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

vaddr_t p_argc, p_envp, p_argv;
/* return argc, or -1 to indicate error */
static int ustack_init(struct proc *p, pagetable_t pagetable, struct commit *commit, vaddr_t sp, char *const argv[], char *const envp[]) {
    /* sp has to be the top address of a page */
    ASSERT(sp % PGSIZE == 0);
    vaddr_t stackbase;
    stackbase = sp - PGSIZE;

    /*       Initial Process Stack (follows SYSTEM V ABI)
        +---------------------------+ <-- High Address
        |     Information block     |
        +---------------------------+
        |           ...             |
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
            paddr_t cp = getphyaddr(p->pagetable, (vaddr_t)argv[argc]);
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
            paddr_t cp = getphyaddr(p->pagetable, (vaddr_t)envp[envpc]);
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
    if ((sp % 16 != 0 && (uint64)(argc + envpc + 2 + 1) % 2 == 0)
        || ((sp % 16 == 0) && (uint64)(argc + envpc + 2 + 1) % 2 == 1)) {
        // Log("aligned");
        sp -= 8;
    }

    /* pad 0 */
    uint8 temp = 0;
    sp -= 8;
    if (copyout(pagetable, sp, (char *)&temp, sizeof(uint8))) {
        return -1;
    }

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

    /* pad 0 */
    sp -= 8;
    if (copyout(pagetable, sp, (char *)&temp, sizeof(uint8))) {
        return -1;
    }

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
    // print_ustack(pagetable, stackbase + PGSIZE);
    return argc;
}

void print_ustack(pagetable_t pagetable, uint64 stacktop) {
    char *pa = (char *)getphyaddr(pagetable, stacktop - 1) + 1;
    // Log("pa is %p", pa);
    /* just print the first 200 8bits of the ustack */
    for (int i = 8; i < 200; i += 8) {
        if (i % 16 == 0) {
            printfGreen("aligned -> ");
        } else {
            printfGreen("           ");
        }

        if (stacktop - i == p_argc || stacktop - i == p_argv || stacktop - i == p_envp) {
            printfBlue("%#x:", stacktop - i);
        } else {
            printfGreen("%#x:", stacktop - i);
        }

        for (int j = 0; j < 8; j++) {
            printfGreen("%c", (char)*(paddr_t *)(pa - i + j));
        }
        printf("\t");
        for (int j = 0; j < 8; j++) {
            printf("%02x ", (uint8) * (paddr_t *)(pa - i + j));
        }
        printf("\n");
    }
}

/* return the end of the virtual address after load segments, or 0 to indicate error */
static uint64 loader(char *path, pagetable_t pagetable, struct commit *commit) {
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
        paddr_t aligned_vaddr = PGROUNDDOWN(ph.vaddr);
        if (ph.vaddr % PGSIZE != 0) {
            paddr_t pa = (paddr_t)kmalloc(PGSIZE);
            if (mappages(pagetable, aligned_vaddr, PGSIZE, pa, flags2perm(ph.flags) | PTE_U, COMMONPAGE) < 0) {
                Warn("misaligned loader mappages failed");
                kfree((void *)pa);
                goto unlock_put;
            }
            off = ph.vaddr - aligned_vaddr;
            size = PGROUNDUP(ph.vaddr) - ph.vaddr;
            if (fat32_inode_read(ip, 0, (uint64)pa + off, ph.off, size) != size) {
                goto unlock_put;
            }
            // Log("entry is %p", elf.entry);
        } else {
            ASSERT(aligned_vaddr == ph.vaddr);
        }
        uint64 sz1;
        ASSERT((ph.vaddr + size) % PGSIZE == 0);
        // Log("\nstart end:%p %p", ph.vaddr + size, ph.vaddr + ph.memsz);
        if ((sz1 = uvmalloc(pagetable, ph.vaddr + size, ph.vaddr + ph.memsz, flags2perm(ph.flags))) == 0)
            goto unlock_put;
        // vmprint(pagetable, 1, 0, 0, 0);
        sz = sz1;
        if (loadseg(pagetable, ph.vaddr + size, ip, ph.off + size, ph.filesz - size) < 0)
            goto unlock_put;
    }
    fat32_inode_unlock_put(ip);
    ip = 0;
    commit->entry = elf.entry;
    return sz;

unlock_put:
    proc_freepagetable(pagetable, sz, 0);
    fat32_inode_unlock_put(ip);
    return 0;
}

int do_execve(char *path, char *const argv[], char *const envp[]) {
    struct commit commit;
    struct proc *p = proc_current();
    struct tcb *t = thread_current();
    char *s, *last;
    uint64 sz = 0, sp;
    uint64 oldsz = p->sz;
    pagetable_t pagetable = 0, oldpagetable;
    memset(&commit, 0, sizeof(commit));

    /* Create a new pagetable for execve */
    if ((pagetable = proc_pagetable(p)) == 0) {
        return -1;
    }

    /* Load the ELF_PROG_LOAD segments */
    if ((sz = loader(path, pagetable, &commit)) == 0) {
        return -1;
    }

    /* Allocate two pages at the next page boundary.
     * Make the first inaccessible as a stack guard.
     * Use the second as the user stack.
     */
    sz = PGROUNDUP(sz);
    uint64 sz1;
    if ((sz1 = uvmalloc(pagetable, sz, sz + 2 * PGSIZE, PTE_W)) == 0)
        goto bad;
    sz = sz1;
    uvmclear(pagetable, sz - 2 * PGSIZE);
    sp = sz;

    /* Initialize the stack */
    int argc = ustack_init(p, pagetable, &commit, sp, argv, envp);
    if (argc < 0) {
        goto bad;
    }

    /* Save program name for debugging */
    for (last = s = path; *s; s++)
        if (*s == '/')
            last = s + 1;
    safestrcpy(p->name, last, sizeof(p->name));

    /* Commit to the user image */
    // p->trapframe->sp = commit.sp;
    // p->trapframe->a1 = commit.a1;
    // p->trapframe->a2 = commit.a2;
    // p->trapframe->epc = commit.entry; // initial program counter = main

    t->trapframe->sp = commit.sp;
    t->trapframe->a1 = commit.a1;
    t->trapframe->a2 = commit.a2;
    t->trapframe->epc = commit.entry;

    p->sz = sz;

    /* free the old pagetable and commit new pagetable */
    oldpagetable = p->pagetable;
    proc_freepagetable(oldpagetable, oldsz, p->tg->thread_idx);
    p->pagetable = pagetable;
    thread_trapframe(t, 1);

    return argc; // this ends up in a0, the first argument to main(argc, argv)

bad:
    if (pagetable) {
        proc_freepagetable(pagetable, sz, 0);
    }
    return -1;
}