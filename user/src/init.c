// init: The initial user-level program
#define USER
#include "stddef.h"
#include "unistd.h"
#include "stdio.h"
#include "string.h"

char *argv[] = {"/busybox/busybox", "sh", 0};
char *envp[] = {"PATH=/oscomp:/bin:/test:/busybox:/iozone:/lmbench", 0};

#define CONSOLE 1
#define DEV_NULL 2
#define DEV_ZERO 3
#define DEV_CPU_DMA_LATENCY 0

int main(void) {
    int pid, wpid;

    // for /dev/null
    mknod("/dev/null", S_IFCHR, DEV_NULL << 8);
    // for /dev/zero
    mknod("/dev/zero", S_IFCHR, DEV_ZERO << 8);
    // for /dev/cpu_dma_latency
    mknod("/dev/cpu_dma_latency", S_IFCHR, DEV_CPU_DMA_LATENCY << 8);
    // for /dev/shm(libc-test)
    mkdir("/dev/shm", 0666);

    // for /dev/tty
    if (openat(AT_FDCWD, "/dev/tty", O_RDWR) < 0) {
        mknod("/dev/tty", S_IFCHR, CONSOLE << 8);
        openat(AT_FDCWD, "/dev/tty", O_RDWR);
    }

    dup(0); // stdout
    dup(0); // stderr

    printf("\n");
    printf("██╗      ██████╗ ███████╗████████╗██╗    ██╗ █████╗ ██╗  ██╗███████╗██╗   ██╗██████╗ \n");
    printf("██║     ██╔═══██╗██╔════╝╚══██╔══╝██║    ██║██╔══██╗██║ ██╔╝██╔════╝██║   ██║██╔══██╗\n");
    printf("██║     ██║   ██║███████╗   ██║   ██║ █╗ ██║███████║█████╔╝ █████╗  ██║   ██║██████╔╝\n");
    printf("██║     ██║   ██║╚════██║   ██║   ██║███╗██║██╔══██║██╔═██╗ ██╔══╝  ██║   ██║██╔═══╝ \n");
    printf("███████╗╚██████╔╝███████║   ██║   ╚███╔███╔╝██║  ██║██║  ██╗███████╗╚██████╔╝██║     \n");
    printf("╚══════╝ ╚═════╝ ╚══════╝   ╚═╝    ╚══╝╚══╝ ╚═╝  ╚═╝╚═╝  ╚═╝╚══════╝ ╚═════╝ ╚═╝     \n");
    printf("\n");

    for (;;) {
        printf("init: starting sh\n");

        pid = fork();
        if (pid < 0) {
            printf("init: fork failed\n");
            exit(1);
        }
        if (pid == 0) {
            printf("about to start sh\n");
            // execve("/bin/sh",argv,envp);
            execve("/busybox/busybox", argv, envp);
            printf("init: exec sh failed\n");
            exit(1);
        }

        for (;;) {
            // this call to wait() returns if the shell exits,
            // or if a parentless process exits.
            wpid = wait((int *)0);
            if (wpid == pid) {
                // the shell exited; restart it.
                break;
            } else if (wpid < 0) {
                printf("init: wait returned an error\n");
                exit(1);
            } else {
                // it was a parentless process; do nothing.
            }
        }
    }
}
