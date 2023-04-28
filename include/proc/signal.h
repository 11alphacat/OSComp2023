#ifndef __SIGNAL_H__
#define __SIGNAL_H__
#include "common.h"
#include "atomic/atomic.h"
#include "atomic/spinlock.h"
#include "list.h"

#define SIGHUP 1
#define SIGINT 2
#define SIGKILL 9
#define SIGSTOP 17
#define SIGTSTP 18
#define SIGTERM 15
#define SIGSEGV 11
#define SIGCHLD 20

typedef void __signalfn_t(int);
typedef __signalfn_t *__sighandler_t;

#define _NSIG 64
#define _NSIG_BPW 64
#define _NSIG_WORDS (_NSIG / _NSIG_BPW)
#define valid_signal(sig) ((sig) <= _NSIG ? 1 : 0)

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

// signal info
typedef struct {
    int si_signo;
    int si_code;
    pid_t si_pid;
} siginfo_t;

// signal sets
typedef struct {
    uint64 sig[_NSIG_WORDS];
} sigset_t;

// the pointer to the signal handler
struct sigaction {
    __sighandler_t sa_handler;
    uint sa_flags;
    sigset_t sa_mask;
};

// signal process
struct signal_struct {
    spinlock_t siglock;
    atomic_t count;
    struct sigaction action[_NSIG];
};

// pending queue
struct sigpending {
    struct list_head list;
    sigset_t signal;
};

struct proc;

int kill(int);
void setkilled(struct proc *);
int killed(struct proc *);


int do_kill(int sig, siginfo_t *info, int pid);
int kill_proc_info(int sig, siginfo_t *info, pid_t pid);
int send_sig_info(int sig, siginfo_t *info, struct proc *p);

#endif