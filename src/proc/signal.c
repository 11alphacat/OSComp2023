// #include "proc/pcb_life.h"
// #include "proc/sched.h"
// #include "proc/signal.h"
// #include "memory/allocator.h"
// #include "errno.h"
// #include "debug.h"
// #include "list.h"
// #include "atomic/atomic.h"

// // delete signals related to the mask in the pending queue
// int signal_queue_pop(uint64 mask, struct sigpending *queue) {
//     ASSERT(queue != NULL);
//     struct sigqueue *sig_cur;
//     struct sigqueue *sig_tmp;

//     if (!sig_test_mask(queue->signal, mask))
//         return 0;

//     sig_del_set_mask(queue->signal, mask);
//     list_for_each_entry_safe(sig_cur, sig_tmp, &queue->list, list) {
//         if (valid_signal(sig_cur->info.si_signo) && (mask & sig_gen_mask(sig_cur->info.si_signo))) {
//             list_del_reinit(&sig_cur->list);
//             // free
//         }
//     }
//     return 1;
// }

// // delete all pending signals of queue
// int signal_queue_flush(struct sigpending *queue) {
//     ASSERT(queue != NULL);
//     struct sigqueue *sig_cur;
//     struct sigqueue *sig_tmp;
//     sig_empty_set(&queue->signal);
//     list_for_each_entry_safe(sig_cur, sig_tmp, &queue->list, list) {
//         list_del_reinit(&sig_cur->list);
//         // free
//     }
//     return 1;
// }

// void signal_info_init(sig_t sig, struct sigqueue *q, siginfo_t *info) {
//     ASSERT(info != NULL);
//     ASSERT(q != NULL);
//     if ((uint64)info == 0) {
//         q->info.si_signo = sig;
//         q->info.si_pid = proc_current()->pid;
//         q->info.si_code = SI_USER;
//     } else if ((uint64)info == 1) {
//         q->info.si_signo = sig;
//         q->info.si_pid = 0;
//         q->info.si_code = SI_KERNEL;
//     } else {
//         q->info = *info;
//     }
// }

// // signal send
// int signal_send(sig_t sig, siginfo_t *info, struct proc *p) {
//     ASSERT(p != NULL);
//     ASSERT(info != NULL);
//     if (sig_ignored(p, sig) || sig_existed(p, sig)) {
//         return 0;
//     }
//     struct sigqueue *q;
//     if ((q = (struct sigqueue *)kalloc()) == NULL) {
//         printf("signal_send : no space in heap\n");
//         return 0;
//     }
//     p->sig_pending_cnt = p->sig_pending_cnt + 1;
//     list_add_tail(&q->list, &p->pending.list);
//     sig_add_set(p->pending.signal, sig);

//     return 1;
// }

// void sigpending_init(struct sigpending *sig) {
//     sig_empty_set(&sig->signal);
//     INIT_LIST_HEAD(&sig->list);
// }

// // signal handlle
// int signal_handle(struct proc *p) {
//     if (p->sig_pending_cnt == 0)
//         return 0;

//     struct sigqueue *sig_cur = NULL;
//     struct sigqueue *sig_tmp = NULL;
//     // struct sighand* sig_hand=NULL;
//     struct sigaction *sig_act = NULL;

//     list_for_each_entry_safe(sig_cur, sig_tmp, &p->pending.list, list) {
//         int sig_no = sig_cur->info.si_signo;
//         if (valid_signal(sig_no)) {
//             panic("signal handle : invalid signo\n");
//         }
//         if (sig_ignored(p, sig_no)) {
//             continue;
//         }
//         sig_act = &sig_action(p, sig_no);
//         if (sig_act->sa_handler == SIG_DFL) {
//             signal_DFL(p, sig_no);
//         } else if (sig_act->sa_handler == SIG_IGN) {
//             continue;
//         } else {
//             do_handle(p, sig_no, sig_act);
//         }
//     }

//     return 1;
// }

// int do_handle(struct proc *p, int sig_no, struct sigaction *sig_act) {
//     (*sig_act->sa_handler)(sig_no);
//     return 1;
// }

// void signal_DFL(struct proc *p, int signo) {
//     int cpid;
//     uint64 wstatus = 0;
//     switch (signo) {
//     case SIGKILL:
//         // case SIGSTOP:
//         setkilled(p);
//         break;
//     case SIGCHLD:

//         cpid = waitpid(-1, wstatus, 0);
//         printfRed("child , pid = %d existed with status : %d", cpid, wstatus);
//         break;
//     default:
//         panic("signal DFL : invalid signo\n");
//         break;
//     }
// }
