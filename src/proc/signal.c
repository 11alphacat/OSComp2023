#include "proc/pcb_life.h"
#include "proc/sched.h"
#include "proc/signal.h"
#include "memory/allocator.h"
#include "errno.h"
#include "debug.h"
#include "list.h"
#include "atomic/atomic.h"
#include "kernel/trap.h"

extern uint32 __user_rt_sigreturn[2];

// delete signals related to the mask in the pending queue
int signal_queue_pop(uint64 mask, struct sigpending *queue) {
    ASSERT(queue != NULL);
    struct sigqueue *sig_cur;
    struct sigqueue *sig_tmp;

    if (!sig_test_mask(queue->signal, mask))
        return 0;

    sig_del_set_mask(queue->signal, mask);
    list_for_each_entry_safe(sig_cur, sig_tmp, &queue->list, list) {
        if (valid_signal(sig_cur->info.si_signo) && (mask & sig_gen_mask(sig_cur->info.si_signo))) {
            list_del_reinit(&sig_cur->list);
            // free
        }
    }
    return 1;
}

// delete all pending signals of queue
int signal_queue_flush(struct sigpending *queue) {
    ASSERT(queue != NULL);
    struct sigqueue *sig_cur;
    struct sigqueue *sig_tmp;
    sig_empty_set(&queue->signal);
    list_for_each_entry_safe(sig_cur, sig_tmp, &queue->list, list) {
        list_del_reinit(&sig_cur->list);
        // free
    }
    return 1;
}

void signal_info_init(sig_t sig, struct sigqueue *q, siginfo_t *info) {
    ASSERT(info != NULL);
    ASSERT(q != NULL);
    if ((uint64)info == 0) {
        q->info.si_signo = sig;
        q->info.si_pid = proc_current()->pid;
        q->info.si_code = SI_USER;
    } else if ((uint64)info == 1) {
        q->info.si_signo = sig;
        q->info.si_pid = 0;
        q->info.si_code = SI_KERNEL;
    } else {
        q->info = *info;
    }
}

// signal send
int signal_send(sig_t sig, siginfo_t *info, struct tcb *t) {
    ASSERT(t != NULL);
    ASSERT(info != NULL);
    if (sig_ignored(t, sig) || sig_existed(t, sig)) {
        return 0;
    }
    struct sigqueue *q;
    if ((q = (struct sigqueue *)kalloc()) == NULL) {
        printf("signal_send : no space in heap\n");
        return 0;
    }
    t->sig_pending_cnt = t->sig_pending_cnt + 1;
    list_add_tail(&q->list, &t->pending.list);
    sig_add_set(t->pending.signal, sig);

    return 1;
}

void sigpending_init(struct sigpending *sig) {
    sig_empty_set(&sig->signal);
    INIT_LIST_HEAD(&sig->list);
}

// signal handlle
int signal_handle(struct tcb *t) {
    if (t->sig_pending_cnt == 0)
        return 0;

    struct sigqueue *sig_cur = NULL;
    struct sigqueue *sig_tmp = NULL;
    // struct sighand* sig_hand=NULL;
    struct sigaction *sig_act = NULL;

    list_for_each_entry_safe(sig_cur, sig_tmp, &t->pending.list, list) {
        int sig_no = sig_cur->info.si_signo;
        if (valid_signal(sig_no)) {
            panic("signal handle : invalid signo\n");
        }
        if (sig_ignored(t, sig_no)) {
            continue;
        }
        sig_act = &sig_action(t, sig_no);
        if (sig_act->sa_handler == SIG_DFL) {
            signal_DFL(t, sig_no);
        } else if (sig_act->sa_handler == SIG_IGN) {
            continue;
        } else {
            do_handle(t, sig_no, sig_act);
            break;
        }
    }

    return 1;
}

int do_handle(struct tcb *t, int sig_no, struct sigaction *sig_act) {
    signal_trapframe_setup(t);
    sigset_t *oldset = &(t->blocked);

    int ret = setup_rt_frame(sig_act, sig_no, oldset, t->trapframe);
    return ret;
}

void signal_DFL(struct tcb *t, int signo) {
    int cpid;
    uint64 wstatus = 0;
    switch (signo) {
    case SIGKILL:
        // case SIGSTOP:
        proc_setkilled(t->p);
        break;
    case SIGCHLD:

        cpid = waitpid(-1, wstatus, 0);
        printfRed("child , pid = %d existed with status : %d", cpid, wstatus);
        break;
    default:
        panic("signal DFL : invalid signo\n");
        break;
    }
}

/*
 * POSIX 3.3.1.3:
 *  "Setting a signal action to SIG_IGN for a signal that is
 *   pending shall cause the pending signal to be discarded,
 *   whether or not it is blocked."
 *
 *  "Setting a signal action to SIG_DFL for a signal that is
 *   pending and whose default action is to ignore the signal
 *   (for example, SIGCHLD), shall cause the pending signal to
 *   be discarded, whether or not it is blocked"
 */
int do_sigaction(int sig, struct sigaction *act, struct sigaction *oact) {
    // struct proc *p = proc_current();
    struct tcb *t = thread_current();
    struct sigaction *k;

    if (!valid_signal(sig)) {
        return -1;
    }
    k = &t->sig->action[sig - 1];
    acquire(&t->sig->siglock);
    if (oact)
        *oact = *k;

    if (act) {
        sig_del_set_mask(act->sa_mask, sig_gen_mask(SIGKILL) | sig_gen_mask(SIGSTOP));
        *k = *act;
        // if (sig_handler_ignored(sig_handler(t, sig), sig)) {
        // 	sigemptyset(&mask);
        // 	sigaddset(&mask, sig);
        // 	rm_from_queue_full(&mask, &t->signal->shared_pending);
        // 	do {
        // 		rm_from_queue_full(&mask, &t->pending);
        // 		t = next_thread(t);
        // 	} while (t != current);
        // }
    }
    release(&t->sig->siglock);
    return 0;
}

int do_sigprocmask(int how, sigset_t *set, sigset_t *oldset) {
    // struct proc* p = proc_current();
    struct tcb *t = thread_current();

    acquire(&t->sig->siglock);
    if (oldset)
        *oldset = t->blocked;
    int error = 0;
    switch (how) {
    case SIG_BLOCK:
        t->blocked.sig = sig_or(t->blocked.sig, set->sig);
        break;
    case SIG_UNBLOCK:
        t->blocked.sig = sig_and(t->blocked.sig, set->sig);
        break;
    case SIG_SETMASK:
        t->blocked = *set;
        break;
    default:
        error = -1;
    }
    // TODO(maybe) : sigpending
    release(&t->sig->siglock);
    return error;
}

void *get_sigframe(struct sigaction *sig, struct trapframe *tf, size_t framesize) {
    uint64 sp;
    /* Default to using normal stack */
    sp = tf->sp;
    /*
     * If we are on the alternate signal stack and would overflow it, don't.
     * Return an always-bogus address instead so we will die with SIGSEGV.
     */
    // if (on_sig_stack(sp) && !likely(on_sig_stack(sp - framesize)))
    // 	return (void __user __force *)(-1UL);

    /* This is the X/Open sanctioned signal stack switching. */
    // sp = sigsp(sp, ksig) - framesize;
    sp -= framesize;

    /* Align the stack frame. */
    sp &= ~0xfUL;

    return (void *)sp;
}

int setup_rt_frame(struct sigaction *sig, int signo, sigset_t *set, struct trapframe *tf) {
    struct proc *p = proc_current();
    struct rt_sigframe *frame;

    frame = get_sigframe(sig, tf, sizeof(*frame));
    frame->uc.uc_flags = 0;
    frame->uc.uc_link = NULL;
    frame->uc.uc_stack.ss_sp = (void *)tf->sp;
    // frame->uc.uc_mcontext.sc_regs  ;
    frame->uc.uc_sigmask = *set;

    if (copyout(p->pagetable, (uint64)frame->sigreturn_code, (char *)__user_rt_sigreturn, sizeof(frame->sigreturn_code)))
        return -1;

    tf->ra = (uint64)&frame->sigreturn_code;
    tf->epc = (uint64)sig->sa_handler;
    tf->sp = (uint64)frame;
    tf->a0 = (uint64)signo; /* a0: signal number */
                            // tf->a1  = (uint64)(&frame->info); /* a1: siginfo pointer */
    tf->a1 = 0;
    tf->a2 = (uint64)(&frame->uc); /* a2: ucontext pointer */
    return 0;
}

void signal_trapframe_setup(struct tcb *t) {
    t->sig_trapframe = *(t->trapframe);
}

void signal_trapframe_restore(struct tcb *t) {
    *(t->trapframe) = t->sig_trapframe;
}