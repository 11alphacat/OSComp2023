#ifndef __OPTIONS_H__
#define __OPTIONS_H__

// CLONE options
#define CLONE_VM 0x00000100             /* set if VM shared between processes */
#define CLONE_FILES 0x00000400          /* Set if open files shared between processes */
#define CLONE_FS 0x00000200             /* set if fs info shared between processes */
#define CLONE_THREAD 0x00010000         /* Set to add to same thread group */
#define CLONE_SIGHAND 0x00000800        /* Set if signal handlers shared.  */
#define CLONE_PARENT_SETTID 0x00100000  /* Store TID in userlevel buffer before MM copy.*/
#define CLONE_SETTLS 0x00080000         /* Set TLS info.  */
#define CLONE_CHILD_CLEARTID 0x00200000 /* Register exit futex and memory location to clear.  */
#define CLONE_CHILD_SETTID 0x01000000   /* Store TID in userlevel buffer in the child.  */

// waitpid options
#define WNOHANG 0x00000001
#define WUNTRACED 0x00000002
#define WCONTINUED 0x00000008


#define WSTOPPED WUNTRACED
#define WEXITED 0x00000004


#define WNOWAIT 0x01000000     /* Don't reap, just poll status.  */
#define __WNOTHREAD 0x20000000 /* Don't wait on children of other threads in this group */
#define __WALL 0x40000000      /* Wait on all children, regardless of type */
#define __WCLONE 0x80000000    /* Wait only on non-SIGCHLD children */

#endif