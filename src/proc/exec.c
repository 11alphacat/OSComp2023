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
#include "elf.h"

/*
 * This structure is used to hold the arguments that are used when loading binaries.
 */
struct binprm {
    // const char *path;
    int argc, envpc;
    struct inode *ip;
    struct mm_struct *mm;
    uint64 sp;

    uint64 e_entry;  

    /* interpreter */
    int interp;
    Elf64_Ehdr *elf_ex;
    // uint64 e_phnu
    // uint64 e_phoff; /* AT_PHDR = e_entry + e_phoff; */

    /* reserve for xv6_user program */
    uint64 a1, a2;
    // # define MAX_ARG_PAGES	32
    // 	struct page *page[MAX_ARG_PAGES];
    // 	unsigned long p; /* current top of mem */
    // 	unsigned long argmin; /* rlimit marker for copy_strings() */
    // 	struct file *executable; /* Executable to pass to the interpreter */
    // 	struct file *interpreter;
    // 	struct file *file;
    // 	struct cred *cred;	/* new credentials */
    // 	int unsafe;		/* how unsafe this exec is (mask of LSM_UNSAFE_*) */
    // 	unsigned int per_clear;	/* bits to clear in current->personality */
    // 	int argc, envc;
    // 	const char *filename;	/* Name of binary as seen by procps */
    // 	const char *interp;	/* Name of the binary really executed. Most
    // 				   of the time same as filename, but could be
    // 				   different for binfmt_{misc,script} */
    // 	const char *fdpath;	/* generated filename for execveat */
    // 	unsigned interp_flags;
    // 	int execfd;		/* File descriptor of the executable */
    // 	unsigned long loader, exec;

    // 	struct rlimit rlim_stack; /* Saved RLIMIT_STACK used during exec. */

    // 	char buf[BINPRM_BUF_SIZE];
};
uint64 load_dyn(char *path);
void map_dyn_loader(pagetable_t pagetable);
void print_ustack(pagetable_t pagetable, uint64 stacktop);
// /* this will commit to trapframe after execve success */
// struct commit {
//     uint64 entry; /* epc */
//     uint64 a1;
//     uint64 a2;
//     uint64 sp;
// };

#define AUX_CNT 38
// typedef struct {
//     int a_type;
//     union {
//         long a_val;
//         void *a_ptr;
//         void (*a_fnc)();
//     } a_un;
// } auxv_t;

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
        // TODO, replace with elf_read
        if (fat32_inode_read(ip, 0, (uint64)pa, offset + i, n) != n)
            return -1;
    }

    return 0;
}

vaddr_t p_argc, p_envp, p_argv, p_auxv;
/* return argc, or -1 to indicate error */
static int ustack_init(struct proc *p, pagetable_t pagetable, struct binprm *bprm, char *const argv[], char *const envp[]) {
    paddr_t spp = getphyaddr(pagetable, USTACK + USTACK_PAGE * PGSIZE - 1) + 1;
    paddr_t stacktop = spp;
    paddr_t stackbase = spp - USTACK_PAGE * PGSIZE;
#define SPP2SP (USTACK + USTACK_PAGE * PGSIZE - (stacktop - spp)) /* only use this macro in this func! */
    Log("%p %p", spp, stackbase);

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
    paddr_t cp;
    /* Information block */
    if (argv != 0) {
        for (; argv[argc]; argc++) {
            if (argc >= MAXARG)
                return -1;
            if ((cp = getphyaddr(p->mm->pagetable, (vaddr_t)argv[argc])) == 0) {
                return -1;
            }
            spp -= strlen((const char *)cp) + 1;
            spp -= spp % 8;
            if (spp < stackbase)
                return -1;
            memmove((void *)spp, (void *)cp, strlen((const char *)cp) + 1);
            argv_addr[argc] = SPP2SP;
        }
    }
    argv_addr[argc] = 0;

    if (envp != 0) {
        for (; envp[envpc]; envpc++) {
            if (envpc >= MAXENV)
                return -1;
            if ((cp = getphyaddr(p->mm->pagetable, (vaddr_t)envp[envpc])) == 0) {
                return -1;
            }
            spp -= strlen((const char *)cp) + 1;
            spp -= spp % 8;
            if (spp < stackbase)
                return -1;
            memmove((void *)spp, (void *)cp, strlen((const char *)cp) + 1);
            envp_addr[envpc] = SPP2SP;
        }
    }
    envp_addr[envpc] = 0;

    ASSERT(spp % 8 == 0);
    // Log("%d %d", sp % 16, (uint64)(argc + envpc + 2 + 1) % 2);
    /* to make sp 16-bit aligned */
    if ((spp % 16 != 0 && (uint64)(argc + envpc + 1 + AUX_CNT) % 2 == 0)
        || ((spp % 16 == 0) && (uint64)(argc + envpc + 2 + 1) % 2 == 1)) {
        // Log("aligned");
        spp -= 8;
    }

    /* auxiliary vectors */
    uint64 auxv[AUX_CNT * 2] = {0};
    for (int i = 0; i < AUX_CNT; i++) {
        if (i + 1 > AT_ENTRY) {
            break;
        }
        auxv[i * 2] = i + 1;
    }
    auxv[AT_PAGESZ * 2 - 1] = PGSIZE;
    if (bprm->interp) {
        auxv[AT_BASE * 2 - 1] = LDSO;
#ifdef __DEBUG_LDSO__
        auxv[AT_PHDR * 2 - 1] = 0x20000000 + bprm->elf_ex->e_phoff;
#else
        auxv[AT_PHDR * 2 - 1] = bprm->elf_ex->e_phoff;
#endif
        auxv[AT_PHNUM * 2 - 1] = bprm->elf_ex->e_phnum;
        auxv[AT_PHENT * 2 - 1] = sizeof(Elf64_Phdr);
        auxv[AT_ENTRY * 2 - 1] = bprm->e_entry;
    }
    spp -= AUX_CNT * 16;
    if (spp < stackbase) {
        return -1;
    }
    p_auxv = SPP2SP;
    memmove((void *)spp, (void *)auxv, AUX_CNT * 16);

    /* push the array of envp[] pointers */
    spp -= (envpc + 1) * sizeof(uint64);
    spp -= spp % 8;
    p_envp = SPP2SP;
    // Log("envp %p", sp);
    if (spp < stackbase)
        return -1;
    memmove((void *)spp, (void *)envp_addr, (envpc + 1) * sizeof(uint64));
    bprm->a2 = SPP2SP;

    /* push the array of argv[] pointers */
    spp -= (argc + 1) * sizeof(uint64);
    spp -= spp % 8;
    p_argv = SPP2SP;
    // Log("argv %p", sp);
    if (spp < stackbase)
        return -1;
    memmove((void *)spp, (void *)argv_addr, (argc + 1) * sizeof(uint64));
    bprm->a1 = SPP2SP;
    spp -= 8;
    p_argc = SPP2SP;
    memmove((void *)spp, (void *)&argc, sizeof(uint64));

    bprm->sp = SPP2SP; // initial stack pointer
    // Log("sp is %p", sp);
    // print_ustack(pagetable, sp);
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


#define elf_phdr Elf64_Phdr
#define elf_Ehdr Elf64_Ehdr

static int elf_read(struct inode *ip, void *buf, uint64 size, uint64 offset) {
    uint64 read;

    // ip->i_op->ilock(ip);
    read = ip->i_op->iread(ip, 0, (uint64)buf, offset, size);
    if (unlikely(read != size)) {
        ip->i_op->iunlock(ip);
        return -1;
    }
    return 0;
    // ip->i_op->iunlock(ip);
}

static Elf64_Ehdr *load_elf_ehdr(struct binprm *bprm) {
    struct inode *ip = bprm->ip;
    Elf64_Ehdr *elf_ex = kmalloc(sizeof(Elf64_Ehdr));

    if (elf_read(ip, elf_ex, sizeof(Elf64_Ehdr), 0) < 0) {
        return NULL;
    }

    // CHECK
    if (memcmp(elf_ex->e_ident, ELFMAG, SELFMAG) != 0)
        goto out;
    if (elf_ex->e_type != ET_EXEC && elf_ex->e_type != ET_DYN)
        goto out;

    return elf_ex;
out:
    kfree(elf_ex);
    return NULL;
}

static Elf64_Phdr *load_elf_phdrs(const Elf64_Ehdr *elf_ex, struct inode *ip) {
    Elf64_Phdr *elf_phdata = NULL;
    int retval;
    unsigned int size;

    if (elf_ex->e_phentsize != sizeof(Elf64_Phdr))
        return NULL;

    /* Sanity check the number of program headers... */
    /* ...and their total size. */
    size = sizeof(Elf64_Phdr) * elf_ex->e_phnum;
    if (size == 0 || size > PGSIZE)
        return NULL;

    elf_phdata = kmalloc(size);
    if (!elf_phdata) {
        Warn("load_elf_phdrs: no free mem");
        return NULL;
    }

    /* Read in the program headers */
    retval = elf_read(ip, elf_phdata, size, elf_ex->e_phoff);
    if (retval < 0) {
        goto out;
    }

    return elf_phdata;

out:
    kfree(elf_phdata);
    return NULL;
}

static uint64 START = 0;
static int load_elf_binary(struct binprm *bprm) {
    Elf64_Ehdr *elf_ex;
    Elf64_Phdr *elf_phdata, *elf_phpnt; /* ph poiner */
    struct mm_struct *mm = bprm->mm;
    struct inode *ip = bprm->ip;
    uint64 sz = 0;

    ip->i_op->ilock(bprm->ip);

    elf_ex = load_elf_ehdr(bprm);
    elf_phdata = load_elf_phdrs(elf_ex, bprm->ip);

    elf_phpnt = elf_phdata;
    for (int i = 0; i < elf_ex->e_phnum; i++, elf_phpnt++) {
        if (elf_phpnt->p_type != PT_INTERP)
            continue;
        load_dyn("libc.so");
        map_dyn_loader(bprm->mm->pagetable);
        bprm->interp = 1;
        break;
    }

    // for (int i = 0;)

    elf_phpnt = elf_phdata;
    // Load program into memory.
    for (int i = 0; i < elf_ex->e_phnum; i++, elf_phpnt++) {
        if (elf_phpnt->p_type != PT_LOAD)
            continue;
        if (elf_phpnt->p_memsz < elf_phpnt->p_filesz)
            goto bad;
        if (elf_phpnt->p_vaddr + elf_phpnt->p_memsz < elf_phpnt->p_vaddr)
            goto bad;

        /* offset: start offset in misaligned page
           size: read size of misaligned page
        */
        uint64 offset = 0, size = 0;
        vaddr_t vaddrdown = PGROUNDDOWN(elf_phpnt->p_vaddr);
        if (elf_phpnt->p_vaddr % PGSIZE != 0) {
            // Warn("%p", vaddrdown);
            paddr_t pa = (paddr_t)kmalloc(PGSIZE);
            if (mappages(mm->pagetable, START + vaddrdown, PGSIZE, pa, flags2perm(elf_phpnt->p_flags) | PTE_U, COMMONPAGE) < 0) {
                Warn("misaligned load mappages failed");
                kfree((void *)pa);
                goto bad;
            }
            offset = elf_phpnt->p_vaddr - vaddrdown;
            size = PGROUNDUP(elf_phpnt->p_vaddr) - elf_phpnt->p_vaddr;
            if (ip->i_op->iread(ip, 0, (uint64)pa + offset, elf_phpnt->p_offset, size) != size) {
                goto bad;
            }
            // Log("entry is %p", elf.entry);
        } else {
            ASSERT(vaddrdown == elf_phpnt->p_vaddr);
        }
        uint64 sz1;
        ASSERT((elf_phpnt->p_vaddr + size) % PGSIZE == 0);
        // Log("\nstart end:%p %p", ph.vaddr + size, ph.vaddr + ph.memsz);
        if ((sz1 = uvmalloc(mm->pagetable, START + elf_phpnt->p_vaddr + size, START + elf_phpnt->p_vaddr + elf_phpnt->p_memsz, flags2perm(elf_phpnt->p_flags))) == 0)
            goto bad;
        // vmprint(mm->pagetable, 1, 0, 0, 0);
        sz = sz1;
        if (loadseg(mm->pagetable, elf_phpnt->p_vaddr + size + START, ip, elf_phpnt->p_offset + size, elf_phpnt->p_filesz - size) < 0)
            goto bad;

        vaddr_t vaddrup = PGROUNDUP(elf_phpnt->p_vaddr + elf_phpnt->p_memsz);
        if (vma_map(mm, START + vaddrdown, vaddrup - vaddrdown, flags2vmaperm(elf_phpnt->p_flags), VMA_TEXT) < 0) {
            goto bad;
        }
    }

    // kfree(elf_ex);
    kfree(elf_phdata);
    ip->i_op->iunlock_put(ip);
    // printf("loader: ip %s unlocked! sem.value = %d\n",ip->fat32_i.fname,ip->i_sem.value);
    ip = 0;
    // if (dyn_flags == 1) {
    //     commit->entry = dyn_entry;
    //     Log("loader entry is %p", dyn_entry);
    // } else {
        // commit->entry = elf.e_entry + START;
    // }
    bprm->elf_ex = elf_ex;
    bprm->e_entry = elf_ex->e_entry + START;
    return sz;

bad:
    kfree(elf_ex);
    kfree(elf_phdata);
    bprm->ip->i_op->iunlock_put(bprm->ip);
    return 0;
}

struct dynamic_loader {
    pagetable_t pagetable;
    uint64 size;
    uint64 entry;
} ldso;

int do_execve(char *path, char *const argv[], char *const envp[]) {
    // struct commit commit;
    // memset(&commit, 0, sizeof(commit));
    struct binprm bprm;
    memset(&bprm , 0, sizeof(struct binprm));
    struct proc *p = proc_current();
    struct tcb *t = thread_current();
    vaddr_t brk; /* program_break */

    struct mm_struct *mm, *oldmm = p->mm;
    mm = alloc_mm();
    if (mm == NULL) {
        return -1;
    }
    bprm.mm = mm;
#ifdef __DEBUG_LDSO__
    if (strcmp(path, "/busybox/busybox_d") == 0) {
        START = 0x20000000;
    }
#endif
    bprm.ip = namei(path);
    if (bprm.ip == 0) {
        return -1;
    }

    /* Loader */
    if ((brk = load_elf_binary(&bprm)) == 0) {
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

    int argc = ustack_init(p, mm->pagetable, &bprm, argv, envp);
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
    t->trapframe->sp = bprm.sp;
    t->trapframe->a1 = bprm.a1;
    t->trapframe->a2 = bprm.a2;
    if (bprm.interp) {
        t->trapframe->epc = ldso.entry + LDSO;
    } else {
        t->trapframe->epc = bprm.e_entry;
    }
    // uvm_thread_trapframe(mm->pagetable, 0, (paddr_t)t->trapframe);

    /* free the old pagetable */
    free_mm(oldmm, p->tg->thread_cnt);

    /* commit new mm */
    p->mm = mm;

    kfree(bprm.elf_ex);

    /* TODO */
    thread_trapframe(t, 1);

    /* for debug, print the pagetable and vmas after exec */
    // vmprint(mm->pagetable, 1, 0, 0, 0);
    // print_vma(&mm->head_vma);
    // panic(0);

    return argc; // this ends up in a0, the first argument to main(argc, argv)

bad:
    // TODO
    // Note: cnt is 1!
    free_mm(mm, 0);
    // todo 
    // kfree(ex);
    return -1;
}


uint64 load_dyn(char *path) {
    ldso.pagetable = (pagetable_t)kzalloc(PGSIZE);
    vmprint(ldso.pagetable, 1, 0, 0, 0);
    int i, off;
    uint64 sz = 0;
    Elf64_Ehdr elf;
    Elf64_Phdr ph;
    struct inode *ip;

    if ((ip = namei(path)) == 0) {
        return -1;
    }
    ip->i_op->ilock(ip);
    // Check ELF header
    if (ip->i_op->iread(ip, 0, (uint64)&elf, 0, sizeof(elf)) != sizeof(elf))
        goto unlock_put;

    if (*(uint32 *)(elf.e_ident) != ELF_MAGIC)
        goto unlock_put;

    // Load program into memory.
    for (i = 0, off = elf.e_phoff; i < elf.e_phnum; i++, off += sizeof(ph)) {
        if (ip->i_op->iread(ip, 0, (uint64)&ph, off, sizeof(ph)) != sizeof(ph))
            goto unlock_put;
        if (ph.p_type != PT_LOAD)
            continue;
        if (ph.p_memsz < ph.p_filesz)
            goto unlock_put;
        if (ph.p_vaddr + ph.p_memsz < ph.p_vaddr)
            goto unlock_put;

        /* off: start offset in misaligned page
           size: read size of misaligned page
        */
        uint64 off = 0, size = 0;
        vaddr_t vaddrdown = PGROUNDDOWN(ph.p_vaddr);
        if (ph.p_vaddr % PGSIZE != 0) {
            // Warn("%p", vaddrdown);
            paddr_t pa = (paddr_t)kmalloc(PGSIZE);
            if (mappages(ldso.pagetable, vaddrdown, PGSIZE, pa, flags2perm(ph.p_flags) | PTE_U, COMMONPAGE) < 0) {
                Warn("misaligned loader mappages failed");
                kfree((void *)pa);
                goto unlock_put;
            }
            off = ph.p_vaddr - vaddrdown;
            size = PGROUNDUP(ph.p_vaddr) - ph.p_vaddr;
            // if (fat32_inode_read(ip, 0, (uint64)pa + off, ph.off, size) != size) {
            if (ip->i_op->iread(ip, 0, (uint64)pa + off, ph.p_offset, size) != size) {
                goto unlock_put;
            }
            // Log("entry is %p", elf.entry);
        } else {
            ASSERT(vaddrdown == ph.p_vaddr);
        }
        uint64 sz1;
        ASSERT((ph.p_vaddr + size) % PGSIZE == 0);
        // Log("\nstart end:%p %p", ph.vaddr + size, ph.vaddr + ph.memsz);
        if ((sz1 = uvmalloc(ldso.pagetable, ph.p_vaddr + size, ph.p_vaddr + ph.p_memsz, flags2perm(ph.p_flags))) == 0)
            goto unlock_put;
        // vmprint(mm->pagetable, 1, 0, 0, 0);
        sz = sz1;
        if (loadseg(ldso.pagetable, ph.p_vaddr + size, ip, ph.p_offset + size, ph.p_filesz - size) < 0)
            goto unlock_put;

        // vaddr_t vaddrup = PGROUNDUP(ph.p_vaddr + ph.p_memsz);
        // if (vma_map(mm, START + vaddrdown, vaddrup - vaddrdown, flags2vmaperm(ph.p_flags), VMA_TEXT) < 0) {
        //     goto unlock_put;
        // }
    }
    // fat32_inode_unlock_put(ip);
    ip->i_op->iunlock_put(ip);
    // printf("loader: ip %s unlocked! sem.value = %d\n",ip->fat32_i.fname,ip->i_sem.value);
    ip = 0;
    ldso.size = sz;
    // vmprint(ldso.pagetable, 1, 0, 0, 0);
    ldso.entry = elf.e_entry;

    return 1;
unlock_put:
    // proc_freepagetable(mm->pagetable, sz, 0);
    // fat32_inode_unlock_put(ip);
    ip->i_op->iunlock_put(ip);
    // printf("loader: ip %s unlocked! sem.value = %d\n",ip->fat32_i.fname,ip->i_sem.value);
    return -1;
}

void map_dyn_loader(pagetable_t pagetable) {
    vaddr_t ldva = LDSO;
    paddr_t ldpa;
    pte_t *pte;
    int flags;
    for (vaddr_t i = 0; i < ldso.size; i += PGSIZE, ldva += PGSIZE) {
        if (i == 140 * PGSIZE) {
            Log("hit");
        }
        walk(ldso.pagetable, i, 0, 0, &pte);
        flags = PTE_FLAGS(*pte);
        ldpa = PTE2PA(*pte);
        mappages(pagetable, ldva, PGSIZE, ldpa, flags, 0);
    }
}