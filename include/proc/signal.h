#ifndef __SIGNAL_H__
#define __SIGNAL_H__
#include "common.h"
#include "atomic/atomic.h"
#include "atomic/spinlock.h"
#include "list.h"

#define SIGKILL 9
#define SIGSTOP 17

#define SIGINT 2
#define SIGTSTP 18
#define SIGTERM 15
#define SIGSEGV 11
#define SIGCHLD 20

typedef void __signalfn_t(int);
typedef __signalfn_t *__sighandler_t;
typedef uint32 sig_t;
#define _NSIG 32
#define valid_signal(sig) (((sig) <= _NSIG && (sig) >= 1) ? 1 : 0)

// how to process signal
#define SA_NOCLDSTOP 0x00000004
#define SA_NOCLDWAIT 0x00000020
#define SA_SIGINFO 0x00000040
#define SA_ONSTACK 0x00000001
#define SA_RESETHAND 0x00000010

#define SA_NODEFER 0x00000008
#define SA_NOMASK SA_NODEFER

#define SA_RESETHAND 0x00000010
#define SA_ONESHOT SA_RESETHAND

#define SI_USER 0      /* sent by kill, sigsend, raise */
#define SI_KERNEL 0x80 /* sent by the kernel from somewhere */

#define SIG_DFL ((__sighandler_t)0)  /* default signal handling */
#define SIG_IGN ((__sighandler_t)1)  /* ignore signal */
#define SIG_ERR ((__sighandler_t)-1) /* error return from signal */

// signal info
typedef struct {
    int si_signo;
    int si_code;
    pid_t si_pid;
} siginfo_t;

// signal sets
typedef struct {
    uint32 sig;
} sigset_t;

// the pointer to the signal handler
struct sigaction {
    __sighandler_t sa_handler;
    uint sa_flags;
    sigset_t sa_mask;
};

// signal process
struct sighand {
    spinlock_t siglock;
    atomic_t count;
    struct sigaction action[_NSIG];
};

// pending signal queue head of proc
struct sigpending {
    struct list_head list;
    sigset_t signal;
};

// signal queue struct
struct sigqueue {
    struct list_head list;
    int flags;
    siginfo_t info;
};

// signal bit op
#define sig_empty_set(set) (memset(set, 0, sizeof(sigset_t)))
#define sig_fill_set(set) (memset(set, -1, sizeof(sigset_t)))
#define sig_add_set(set, sig) (set.sig |= 1UL << (sig - 1))
#define sig_del_set(set, sig) (set.sig &= ~(1UL << (sig - 1)))
#define sig_add_set_mask(set, mask) (set.sig |= (mask))
#define sig_del_set_mask(set, mask) (set.sig &= (~mask))
#define sig_is_member(set, n_sig) (1 & (set.sig >> (n_sig - 1)))
#define sig_gen_mask(sig) (1UL << (sig - 1))
#define sig_or(x, y) ((x) | (y))
#define sig_and(x, y) ((x) & (y))
#define sig_test_mask(set, mask) ((set.sig & mask) != 0)
#define sig_pending(p) (p.sig_pending)
#define sig_ignored(p, sig) (sig_is_member(p->blocked, sig))
#define sig_existed(p, sig) (sig_is_member(p->pending.signal, sig))
#define sig_action(p, signo) (p->sighander->action[signo])

// typedef struct _ucontext {
//     unsigned long uc_flags;
//     struct _ucontext *uc_link;
//     stack_t uc_stack;
//     mcontext_t uc_mcontext;
//     sigset_t uc_sigmask;
// } ucontext_t;

struct proc;

// int signal_queue_pop(uint64 mask, struct sigpending *queue);
// int signal_queue_flush(struct sigpending *queue);
// void signal_info_init(sig_t sig, struct sigqueue *q, siginfo_t *info);
// int signal_send(sig_t sig, siginfo_t *info, struct proc *p);
// void sigpending_init(struct sigpending *sig);
// int signal_handle(struct proc *p);
// int do_handle(struct proc *p, int sig_no, struct sigaction *sig_act);
// void signal_DFL(struct proc *p, int signo);

#endif