#define USER
#include "stddef.h"
#include "unistd.h"
#include "stdio.h"
#include "string.h"

// Test that fork fails gracefully.
void forktest(void) {
    int n, pid;
    printf("==========fork test==========\n");
    int N = 100;
    for (n = 0; n < 100; n++) {
        pid = fork();
        if (pid < 0)
            break;
        if (pid == 0)
            exit(0);
    }

    if (n == N) {
        printf("fork claimed to work N times!\n");
        exit(1);
    }

    for (; n > 0; n--) {
        if (wait(0) < 0) {
            printf("wait stopped early\n");
            exit(1);
        }
    }

    if (wait(0) != -1) {
        printf("wait got too many\n");
        exit(1);
    }

    printf("fork test OK\n");
}

// concurrent forks to try to expose locking bugs.
void forkfork() {
    int N = 2;
    printf("==========forkfork test==========\n");
    for (int i = 0; i < N; i++) {
        int pid = fork();
        if (pid < 0) {
            printf("fork failed");
            exit(1);
        }
        if (pid == 0) {
            for (int j = 0; j < 100; j++) {
                int pid1 = fork();
                if (pid1 < 0) {
                    exit(1);
                }
                if (pid1 == 0) {
                    exit(0);
                }
                wait(0);
            }
            exit(0);
        }
    }

    int xstatus;
    for (int i = 0; i < N; i++) {
        wait(&xstatus);
        if (xstatus != 0) {
            printf("fork in child failed");
            exit(1);
        }
    }
    printf("forkfork test OK\n");
}

// simple file system tests (open syscall)
void opentest() {
    int fd;
    printf("==========open test==========\n");

    fd = open("echo", 0);
    if (fd < 0) {
        printf("open echo failed!\n");
        exit(1);
    }
    close(fd);
    fd = open("doesnotexist", 0);
    if (fd >= 0) {
        printf("open doesnotexist succeeded!\n");
        exit(1);
    }
    printf("open test OK\n");
}

// try to find any races between exit and wait
void exitwait() {
    int i, pid;
    printf("==========exitwait test==========\n");

    for (i = 0; i < 1000; i++) {
        pid = fork();
        if (pid < 0) {
            printf("fork failed\n");
            exit(1);
        }
        if (pid) {
            int exit_state;
            if (wait(&exit_state) != pid) {
                printf("wait wrong pid\n");
                exit(1);
            }
            if (i != exit_state) {
                printf("wait wrong exit status\n");
                exit(1);
            }
        } else {
            exit(i);
        }
    }
    printf("exitwait test OK\n");
}

// what if two children exit() at the same time?
void twochildren() {
    printf("==========twochildren test==========\n");
    for (int i = 0; i < 1000; i++) {
        int pid1 = fork();
        if (pid1 < 0) {
            printf("fork failed\n");
            exit(1);
        }
        if (pid1 == 0) {
            exit(0);
        } else {
            int pid2 = fork();
            if (pid2 < 0) {
                printf("fork failed\n");
                exit(1);
            }
            if (pid2 == 0) {
                exit(0);
            } else {
                wait(0);
                wait(0);
            }
        }
    }
    printf("twochildren test OK\n");
}

// try to find races in the reparenting
// code that handles a parent exiting
// when it still has live children.
void reparent() {
    printf("==========reparent test==========\n");
    int master_pid = getpid();
    for (int i = 0; i < 200; i++) {
        int pid = fork();
        if (pid < 0) {
            printf("fork failed\n");
            exit(1);
        }
        if (pid) {
            if (wait(0) != pid) {
                printf("wait wrong pid\n");
                exit(1);
            }
        } else {
            int pid2 = fork();
            if (pid2 < 0) {
                kill(master_pid);
                exit(1);
            }
            exit(0);
        }
    }
    printf("reparent test OK\n");
}

// regression test. does reparent() violate the parent-then-child
// locking order when giving away a child to init, so that exit()
// deadlocks against init's wait()? also used to trigger a "panic:
// release" due to exit() releasing a different p->parent->lock than
// it acquired.
void reparent2() {
    printf("==========reparent2 test==========\n");
    for (int i = 0; i < 400; i++) {
        int pid1 = fork();
        if (pid1 < 0) {
            printf("fork failed\n");
            exit(1);
        }
        if (pid1 == 0) {
            fork();
            fork();
            exit(0);
        }
        wait(0);
    }
    printf("reparent2 test OK\n");
}

#define MAXOPBLOCKS 10
#define BSIZE 512
#define BUFSZ ((MAXOPBLOCKS + 2) * BSIZE)
char buf[BUFSZ];

// write syscall
void writetest() {
    int fd;
    int N = 100;
    int SZ = 10;
    printf("==========write test==========\n");
    fd = open("small", O_CREATE | O_RDWR);
    if (fd < 0) {
        printf("error: create small failed!\n");
        exit(1);
    }

    for (int i = 0; i < N; i++) {
        if (write(fd, "aaaaaaaaaa", SZ) != SZ) {
            printf("error: write aa %d new file failed\n", i);
            exit(1);
        }
        if (write(fd, "bbbbbbbbbb", SZ) != SZ) {
            printf("error: write bb %d new file failed\n", i);
            exit(1);
        }
    }
    close(fd);
    fd = open("small", O_RDONLY);
    if (fd < 0) {
        printf("error: open small failed!\n");
        exit(1);
    }
    int ret = read(fd, buf, N * SZ * 2);
    if (ret != N * SZ * 2) {
        printf("read failed\n");
        exit(1);
    }
    close(fd);
    printf("write test OK\n");
}

void writebig() {
    int i, fd, n;
    int MAXFILE = 2048;
    printf("==========writebig test==========\n");
    fd = open("big", O_CREATE | O_RDWR);
    if (fd < 0) {
        printf("error: creat big failed!\n");
        exit(1);
    }

    for (i = 0; i < MAXFILE; i++) {
        ((int *)buf)[0] = i;
        if (write(fd, buf, BSIZE) != BSIZE) {
            printf("error: write big file failed\n", i);
            exit(1);
        }
    }
    close(fd);

    fd = open("big", O_RDONLY);
    if (fd < 0) {
        printf("error: open big failed!\n");
        exit(1);
    }
    n = 0;
    for (;;) {
        i = read(fd, buf, BSIZE);
        if (i == 0) {
            if (n == MAXFILE - 1) {
                printf("read only %d blocks from big", n);
                exit(1);
            }
            break;
        } else if (i != BSIZE) {
            printf("read failed %d\n", i);
            exit(1);
        }
        if (((int *)buf)[0] != n) {
            printf("read content of block %d is %d\n", n, ((int *)buf)[0]);
            exit(1);
        }
        n++;
    }
    close(fd);
    if (unlink("big") < 0) {
        printf("unlink big failed\n");
        exit(1);
    }
    printf("writebig test OK\n");
}

void openiputtest() {
    int pid, xstatus;
    printf("==========openiputtest test==========\n");

    if (mkdir("oidir", 0666) < 0) {
        printf("mkdir oidir failed\n");
        exit(1);
    }
    pid = fork();
    if (pid < 0) {
        printf("fork failed\n");
        exit(1);
    }
    if (pid == 0) {
        int fd = open("oidir", O_RDWR);
        if (fd >= 0) {
            printf("open directory for write succeeded\n");
            exit(1);
        }
        exit(0);
    }

    sleep(1);
    if (unlink("oidir") != 0) {
        printf("unlink failed\n");
        exit(1);
    }

    wait(&xstatus);
    printf("openiputtest test OK\n");
}

void truncate1() {
    char buf[32];
    printf("==========truncate1 test==========\n");

    unlink("truncfile");
    int fd1 = open("truncfile", O_CREATE | O_WRONLY | O_TRUNC);

    write(fd1, "abcd", 4);
    close(fd1);

    int fd2 = open("truncfile", O_RDONLY);

    memset(buf, 0, sizeof(buf));
    int n = read(fd2, buf, sizeof(buf));
    printf("%s\n", buf);
    if (n != 4) {
        printf("read %d bytes, wanted 4\n", n);
        exit(1);
    }

    fd1 = open("truncfile", O_WRONLY | O_TRUNC);
    int fd3 = open("truncfile", O_RDONLY);

    memset(buf, 0, sizeof(buf));
    n = read(fd3, buf, sizeof(buf));
    printf("%s\n", buf);

    if (n != 0) {
        printf("aaa fd3=%d\n", fd3);
        printf("read %d bytes, wanted 0\n", n);
        exit(1);
    }

    // memset(buf, 0, sizeof(buf));
    n = read(fd2, buf, sizeof(buf));
    // printf("%s\n",buf);
    if (n != 0) {
        printf("bbb fd2=%d\n", fd2);
        printf("read %d bytes, wanted 0\n", n);
        exit(1);
    }

    write(fd1, "abcdef", 6);

    memset(buf, 0, sizeof(buf));
    n = read(fd3, buf, sizeof(buf));
    printf("%s\n", buf);

    if (n != 6) {
        printf("read %d bytes, wanted 6\n", n);
        exit(1);
    }

    memset(buf, 0, sizeof(buf));
    n = read(fd2, buf, sizeof(buf));
    printf("%s\n", buf);

    if (n != 2) {
        printf("read %d bytes, wanted 2\n", n);
        exit(1);
    }

    unlink("truncfile");

    close(fd1);
    close(fd2);
    close(fd3);

    printf("truncate1 test OK\n");
}

void forkforkfork() {
    printf("==========forkforkfork test==========\n");
    unlink("stopforking");
    int pid = fork();
    if (pid < 0) {
        printf("fork failed");
        exit(1);
    }
    if (pid == 0) {
        while (1) {
            int fd = open("stopforking", 0);
            if (fd >= 0) {
                exit(0);
            }
            if (fork() < 0) {
                close(open("stopforking", O_CREATE | O_RDWR));
            }
        }
        exit(0);
    }

    sleep(2); // two seconds
    close(open("stopforking", O_CREATE | O_RDWR));
    wait(0);
    sleep(1); // one second

    printf("forkforkfork test OK\n");
}

void preempt() {
    printf("==========preempt test==========\n");
    int pid1, pid2, pid3;
    int pfds[2];

    pid1 = fork();
    if (pid1 < 0) {
        printf("fork failed");
        exit(1);
    }
    if (pid1 == 0)
        for (;;)
            ;

    pid2 = fork();
    if (pid2 < 0) {
        printf("fork failed\n");
        exit(1);
    }
    if (pid2 == 0)
        for (;;)
            ;

    pipe(pfds);
    pid3 = fork();
    if (pid3 < 0) {
        printf("fork failed\n");
        exit(1);
    }
    if (pid3 == 0) {
        close(pfds[0]);
        if (write(pfds[1], "x", 1) != 1)
            printf("preempt write error");
        close(pfds[1]);
        for (;;)
            ;
    }

    close(pfds[1]);
    if (read(pfds[0], buf, sizeof(buf)) != 1) {
        printf("preempt read error");
        return;
    }
    close(pfds[0]);
    printf("kill... ");
    kill(pid1);
    kill(pid2);
    kill(pid3);
    printf("wait... ");
    wait(0);
    wait(0);
    wait(0);

    printf("preempt test OK\n");
}

// test if child is killed (status = -1)
void killstatus() {
    printf("==========killstatus test==========\n");
    int xst;

    for (int i = 0; i < 100; i++) {
        int pid1 = fork();
        if (pid1 < 0) {
            printf("fork failed\n");
            exit(1);
        }
        if (pid1 == 0) {
            while (1) {
                getpid();
            }
            exit(0);
        }
        sleep(1);
        kill(pid1);
        wait(&xst);
        if (xst != -1) {
            printf("status should be -1\n");
            exit(1);
        }
        printf("%d\n", i);
    }
    printf("killstatus test OK\n");
}

char buf2[4096];
int fds[2];

// test whether copyout() simulates COW faults.
void filetest() {
    printf("==========COW faults test==========\n");

    buf2[0] = 99;

    for (int i = 0; i < 4; i++) {
        if (pipe(fds) != 0) {
            printf("pipe() failed\n");
            exit(-1);
        }
        int pid = fork();
        if (pid < 0) {
            printf("fork failed\n");
            exit(-1);
        }
        if (pid == 0) {
            sleep(1);
            if (read(fds[0], buf2, sizeof(i)) != sizeof(i)) {
                printf("error: read failed\n");
                exit(1);
            }
            sleep(1);
            int j = *(int *)buf2;
            if (j != i) {
                printf("error: read the wrong value\n");
                exit(1);
            }
            exit(0);
        }
        if (write(fds[1], &i, sizeof(i)) != sizeof(i)) {
            printf("error: write failed\n");
            exit(-1);
        }
    }

    int xstatus = 0;
    for (int i = 0; i < 4; i++) {
        wait(&xstatus);
        if (xstatus != 0) {
            exit(1);
        }
    }

    if (buf2[0] != 99) {
        printf("error: child overwrote parent\n");
        exit(1);
    }

    printf("COW faults test OK\n");
}

// what if you pass ridiculous pointers to system calls
// that read user memory with copyin?
void copyin() {
    printf("==========copyin test==========\n");
    // print_pgtable();
    uint64 addrs[] = {0x80000000LL, 0xffffffffffffffff};

    for (int ai = 0; ai < 2; ai++) {
        uint64 addr = addrs[ai];

        int fd = open("copyin1", O_CREATE | O_WRONLY);
        if (fd < 0) {
            printf("open(copyin1) failed\n");
            exit(1);
        }
        int n = write(fd, (void *)addr, 8192);
        if (n >= 0) {
            printf("write(fd, %p, 8192) returned %d, not -1\n", addr, n);
            exit(1);
        }
        close(fd);
        unlink("copyin1");

        n = write(1, (char *)addr, 8192);
        if (n > 0) {
            printf("write(1, %p, 8192) returned %d, not -1 or 0\n", addr, n);
            exit(1);
        }

        int fds[2];
        if (pipe(fds) < 0) {
            printf("pipe() failed\n");
            exit(1);
        }
        n = write(fds[1], (char *)addr, 8192);
        if (n > 0) {
            printf("write(pipe, %p, 8192) returned %d, not -1 or 0\n", addr, n);
            exit(1);
        }
        close(fds[0]);
        close(fds[1]);
    }
    printf("copyin test OK\n");
}

// what if you pass ridiculous pointers to system calls
// that write user memory with copyout?
void copyout() {
    printf("==========copyout test==========\n");
    uint64 addrs[] = {0x80000000LL, 0xffffffffffffffff};

    for (int ai = 0; ai < 2; ai++) {
        uint64 addr = addrs[ai];

        int fd = open("READ.md", 0);
        if (fd < 0) {
            printf("open(READ.md) failed\n");
            exit(1);
        }
        int n = read(fd, (void *)addr, 8192);
        if (n > 0) {
            printf("read(fd, %p, 8192) returned %d, not -1 or 0\n", addr, n);
            exit(1);
        }
        close(fd);

        int fds[2];
        if (pipe(fds) < 0) {
            printf("pipe() failed\n");
            exit(1);
        }
        n = write(fds[1], "x", 1);
        if (n != 1) {
            printf("pipe write failed\n");
            exit(1);
        }
        n = read(fds[0], (void *)addr, 8192);
        if (n > 0) {
            printf("read(pipe, %p, 8192) returned %d, not -1 or 0\n", addr, n);
            exit(1);
        }
        close(fds[0]);
        close(fds[1]);
    }
    printf("copyout test OK\n");
}

// what if you pass ridiculous string pointers to system calls?
void copyinstr1() {
    printf("==========copyinstr1 test==========\n");
    uint64 addrs[] = {0x80000000LL, 0xffffffffffffffff};

    for (int ai = 0; ai < 2; ai++) {
        uint64 addr = addrs[ai];

        int fd = open((char *)addr, O_CREATE | O_WRONLY);
        if (fd >= 0) {
            printf("open(%p) returned %d, not -1\n", addr, fd);
            exit(1);
        }
    }
    printf("copyinstr1 test OK\n");
}


// See if the kernel refuses to read/write user memory that the
// application doesn't have anymore, because it returned it.
void rwsbrk() {
    int fd, n;
    printf("==========rwsbrk test==========\n");

    uint64 a = (uint64)sbrk(8192);

    if (a == 0xffffffffffffffffLL) {
        printf("sbrk(rwsbrk) failed\n");
        exit(1);
    }

    if ((uint64)sbrk(-8192) == 0xffffffffffffffffLL) {
        printf("sbrk(rwsbrk) shrink failed\n");
        exit(1);
    }

    fd = open("rwsbrk", O_CREATE | O_WRONLY);
    if (fd < 0) {
        printf("open(rwsbrk) failed\n");
        exit(1);
    }
    n = write(fd, (void *)(a + 4096), 1024);
    if (n >= 0) {
        printf("write(fd, %p, 1024) returned %d, not -1\n", a + 4096, n);
        exit(1);
    }
    close(fd);
    unlink("rwsbrk");

    fd = open("READ.md", O_RDONLY);
    if (fd < 0) {
        printf("open(rwsbrk) failed\n");
        exit(1);
    }
    n = read(fd, (void *)(a + 4096), 10);
    if (n >= 0) {
        printf("read(fd, %p, 10) returned %d, not -1\n", a + 4096, n);
        exit(1);
    }
    close(fd);
    printf("rwsbrk test OK\n");
}

// write to an open FD whose file has just been truncated.
// this causes a write at an offset beyond the end of the file.
// such writes fail on xv6 (unlike POSIX) but at least
// they don't crash.
void truncate2() {
    printf("==========truncate2 test==========\n");
    unlink("truncfile");

    int fd1 = open("truncfile", O_CREATE | O_TRUNC | O_WRONLY);
    write(fd1, "abcd", 4);

    int fd2 = open("truncfile", O_TRUNC | O_WRONLY);

    int n = write(fd1, "x", 1);
    if (n != -1) {
        printf("write returned %d, expected -1\n", n);
        exit(1);
    }

    unlink("truncfile");
    close(fd1);
    close(fd2);
    printf("truncate2 test OK\n");
}

void truncate3() {
    printf("==========truncate3 test==========\n");
    int pid, xstatus;

    close(open("truncfile", O_CREATE | O_TRUNC | O_WRONLY));

    pid = fork();
    if (pid < 0) {
        printf("fork failed\n");
        exit(1);
    }

    if (pid == 0) {
        for (int i = 0; i < 100; i++) {
            char buf[32];
            int fd = open("truncfile", O_WRONLY);
            if (fd < 0) {
                printf("open failed\n");
                exit(1);
            }
            int n = write(fd, "1234567890", 10);
            if (n != 10) {
                printf("write got %d, expected 10\n", n);
                exit(1);
            }
            close(fd);
            fd = open("truncfile", O_RDONLY);
            read(fd, buf, sizeof(buf));
            close(fd);
        }
        exit(0);
    }

    for (int i = 0; i < 150; i++) {
        int fd = open("truncfile", O_CREATE | O_WRONLY | O_TRUNC);
        if (fd < 0) {
            printf("open failed\n");
            exit(1);
        }
        int n = write(fd, "xxx", 3);
        if (n != 3) {
            printf("write got %d, expected 3\n", n);
            exit(1);
        }
        close(fd);
    }

    wait(&xstatus);
    unlink("truncfile");
    printf("truncate3 test OK\n");
}

// does chdir() call iput(p->cwd) in a transaction?
void iputtest() {
    printf("==========iput test==========\n");
    if (mkdir("iputdir", 0666) < 0) {
        printf("mkdir failed\n");
        exit(1);
    }
    if (chdir("iputdir") < 0) {
        printf("chdir iputdir failed\n");
        exit(1);
    }
    if (unlink("../iputdir") < 0) {
        printf("unlink ../iputdir failed\n");
        exit(1);
    }
    if (chdir("/") < 0) {
        printf("chdir / failed\n");
        exit(1);
    }
    printf("iput test OK\n");  
}

// does exit() call iput(p->cwd) in a transaction?
void exitiputtest() {
    int pid, xstatus;
    printf("==========exitiput test==========\n");
    pid = fork();
    if (pid < 0) {
        printf("fork failed\n");
        exit(1);
    }
    if (pid == 0) {
        if (mkdir("iputdir", 0666) < 0) {
            printf("mkdir failed\n");
            exit(1);
        }
        printf("???\n");
        if (chdir("iputdir") < 0) {
            printf("child chdir failed\n");
            exit(1);
        }
        if (unlink("../iputdir") < 0) {
            printf("unlink ../iputdir failed\n");
            exit(1);
        }
        exit(0);
    }
    wait(&xstatus);
    printf("exitiput test OK\n");   
}

// many creates, followed by unlink test
void createtest() {
    printf("==========createtest test==========\n");
    int i, fd;
    int N = 3;
    char name[3];
    name[0] = 'a';
    name[2] = '\0';
    for (i = 0; i < N; i++) {
        name[1] = '0' + i;
        fd = open(name, O_CREATE | O_RDWR);
        close(fd);
    }
    // name[0] = 'a';
    // name[2] = '\0';
    // for (i = 0; i < N; i++) {
    //     name[1] = '0' + i;
    //     unlink(name);
    // }
    printf("createtest test OK\n"); 
}



// if process size was somewhat more than a page boundary, and then
// shrunk to be somewhat less than that page boundary, can the kernel
// still copyin() from addresses in the last page?
void sbrklast() {
    printf("==========sbrklast test==========\n");
    uint64 top = (uint64)sbrk(0);
    if ((top % 4096) != 0)
        sbrk(4096 - (top % 4096));
    sbrk(4096);
    sbrk(10);
    sbrk(-20);
    top = (uint64)sbrk(0);
    char *p = (char *)(top - 64);
    p[0] = 'x';
    p[1] = '\0';
    int fd = open(p, O_RDWR | O_CREATE);
    write(fd, p, 1);
    close(fd);
    fd = open(p, O_RDWR);
    p[0] = '\0';
    read(fd, p, 1);
    if (p[0] != 'x')
        exit(1);
    printf("sbrklast test OK\n"); 
}

// // Section with tests that take a fair bit of time
// // directory that uses indirect blocks
// void bigdir() {
//     printf("==========bigdir test==========\n");
//     enum { N = 500 };
//     int i, fd;
//     char name[10];

//     unlink("bd");

//     fd = open("bd", O_CREATE);
//     if (fd < 0) {
//         printf("bigdir create failed\n");
//         exit(1);
//     }
//     close(fd);

//     for (i = 0; i < N; i++) {
//         name[0] = 'x';
//         name[1] = '0' + (i / 64);
//         name[2] = '0' + (i % 64);
//         name[3] = '\0';
//         if (link("bd", name) != 0) {
//             printf("bigdir link(bd, %s) failed\n", name);
//             exit(1);
//         }
//     }

//     unlink("bd");
//     for (i = 0; i < N; i++) {
//         name[0] = 'x';
//         name[1] = '0' + (i / 64);
//         name[2] = '0' + (i % 64);
//         name[3] = '\0';
//         if (unlink(name) != 0) {
//             printf("bigdir unlink failed\n");
//             exit(1);
//         }
//     }
//     printf("bigdir test OK\n"); 
// }


void dirtest() {
    printf("==========dir test=========\n");
    if (mkdir("dir0", 0666) < 0) {
        printf("mkdir failed\n");
        exit(1);
    }

    if (chdir("dir0") < 0) {
        printf("chdir dir0 failed\n");
        exit(1);
    }

    if (chdir("..") < 0) {
        printf("chdir .. failed\n");
        exit(1);
    }

    if (unlink("dir0") < 0) {
        printf("unlink dir0 failed\n");
        exit(1);
    }
    printf("dir test OK\n"); 
}


void execvetest() {
    printf("==========execve test=========\n");
    int fd, xstatus, pid;
    char *echoargv[] = {"echo", "OK", 0};
    char buf[3];

    unlink("echo-ok");
    pid = fork();
    if (pid < 0) {
        printf("fork failed\n");
        exit(1);
    }
    if (pid == 0) {
        close(1);
        fd = open("echo-ok", O_CREATE | O_WRONLY);
        if (fd < 0) {
            printf("create failed\n");
            exit(1);
        }
        if (fd != 1) {
            printf("wrong fd\n");
            exit(1);
        }
        if (execve("echo", echoargv, NULL) < 0) {
            printf("execve echo failed\n");
            exit(1);
        }
        // won't get to here
    }

    if (wait(&xstatus) != pid) {
        printf("wait failed!\n");
    }
    if (xstatus != 0)
        exit(xstatus);

    fd = open("echo-ok", O_RDONLY);
    if (fd < 0) {
        printf("open failed\n");
        exit(1);
    }
    if (read(fd, buf, 2) != 2) {
        printf("read failed\n");
        exit(1);
    }
    unlink("echo-ok");
    if (buf[0] == 'O' && buf[1] == 'K'){
        printf("execve test OK\n"); 
    }
    else {
        printf("wrong output\n");
        exit(1);
    }

}

void uvmfree() {
    printf("==========uvmfree test=========\n");
    enum { BIG = 100 * 1024 * 1024 };
    char *a, *p;
    uint64 amt;

    int pid = fork();
    if (pid == 0) {
        // oldbrk = sbrk(0);

        // can one grow address space to something big?
        a = sbrk(0);
        amt = BIG - (uint64)a;
        p = sbrk(amt);
        if (p != a) {
            printf("sbrk test failed to grow big address space; enough phys mem?\n");
            exit(1);
        }
        printf("child done\n");
        exit(0);
    } else if (pid > 0) {
        wait((int *)0);
        printf("uvmfree test OK\n"); 
        exit(0);
    } else {
        printf("fork failed");
        exit(1);
    }

}

int main(void) {
    forktest();
    exitwait();
    forkfork();
    forkforkfork();
    twochildren();
    reparent();
    reparent2();
    killstatus();

    opentest();
    openiputtest();
    writetest();
    writebig();
    preempt();
    truncate1();
    copyin();
    copyout();
    copyinstr1();
    truncate2();
    truncate3();
    iputtest();
    exitiputtest();
    createtest();
    sbrklast();
    dirtest();
    execvetest();
    uvmfree();

    exit(0);
    return 0;
}
