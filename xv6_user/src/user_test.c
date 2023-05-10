#define USER
#include "stddef.h"
#include "unistd.h"
#include "stdio.h"
#include "string.h"

void forktest(void)
{
    int n, pid;
    printf("==========fork test==========\n");
    int N = 100;
    for(n=0; n<100; n++){
        pid = fork();
        if(pid < 0)
            break;
        if(pid == 0)
            exit(0);
    }

    if(n == N){
        printf("fork claimed to work N times!\n");
        exit(1);
    }

    for(; n > 0; n--){
        if(wait(0) < 0){
            printf("wait stopped early\n");
            exit(1);
        }
    }

    if(wait(0) != -1){
        printf("wait got too many\n");
        exit(1);
    }

    printf("fork test OK\n");
}

void forkfork()
{
    int N =2 ;
    printf("==========forkfork test==========\n");
    for(int i = 0; i < N; i++){
        int pid = fork();
        if(pid < 0){
            printf("fork failed");
            exit(1);
        }
        if(pid == 0){
            for(int j = 0; j < 100; j++){
                int pid1 = fork();
                if(pid1 < 0){
                    exit(1);
                }
                if(pid1 == 0){
                    exit(0);
                }
                wait(0);
            }
            exit(0);
        }   
    }   

    int xstatus;
    for(int i = 0; i < N; i++){
        wait(&xstatus);
        if(xstatus != 0) {
            printf("fork in child failed");
            exit(1);
        }
    }
    printf("forkfork test OK\n");
}


void opentest()
{
    int fd;
    printf("==========open test==========\n");

    fd = open("echo", 0);
    if(fd < 0){
        printf("open echo failed!\n");
        exit(1);
    }
    close(fd);
    fd = open("doesnotexist", 0);
    if(fd >= 0){
        printf("open doesnotexist succeeded!\n");
        exit(1);
    }
    printf("open test OK\n");
}

void exitwait()
{
    int i, pid;
    printf("==========exitwait test==========\n");

    for(i = 0; i < 1000; i++){
        pid = fork();
        if(pid < 0){
            printf("fork failed\n");
            exit(1);
        }
        if(pid){
            int exit_state;
            if(wait(&exit_state) != pid){
                printf("wait wrong pid\n");
                exit(1);
            }
            if(i != exit_state) {
                printf("wait wrong exit status\n");
                exit(1);
            }
        } else {
            exit(i);
        }
    }
    printf("exitwait test OK\n");
}

void twochildren()
{
    printf("==========twochildren test==========\n");
    for(int i = 0; i < 1000; i++){
        int pid1 = fork();
        if(pid1 < 0){
            printf("fork failed\n");
            exit(1);
        }
        if(pid1 == 0){
            exit(0);
        } else {
            int pid2 = fork();
            if(pid2 < 0){
                printf("fork failed\n");
                exit(1);
            }
            if(pid2 == 0){
                exit(0);
            } else {
                wait(0);
                wait(0);
            }
        }
    }
    printf("twochildren test OK\n");
}

void reparent2()
{
    printf("==========reparent2 test==========\n");
    for(int i = 0; i < 800; i++){
        int pid1 = fork();
        if(pid1 < 0){
            printf("fork failed\n");
            exit(1);
        }
        if(pid1 == 0){
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
#define BUFSZ  ((MAXOPBLOCKS+2)*BSIZE)
char buf[BUFSZ];

void writetest()
{
    int fd;
    int N=100;
    int SZ=10;
    printf("==========write test==========\n");
    fd = open("small", O_CREATE|O_RDWR);
    if(fd < 0){
        printf("error: create small failed!\n");
        exit(1);
    }

    for(int i = 0; i < N; i++){
        if(write(fd, "aaaaaaaaaa", SZ) != SZ){
            printf("error: write aa %d new file failed\n", i);
            exit(1);
        }
        if(write(fd, "bbbbbbbbbb", SZ) != SZ){
            printf("error: write bb %d new file failed\n", i);
            exit(1);
        }
    }
    close(fd);
    fd = open("small", O_RDONLY);
    if(fd < 0){
        printf("error: open small failed!\n");
        exit(1);
    }
    int ret = read(fd, buf, N*SZ*2);
    if(ret != N*SZ*2){
        printf("read failed\n");
        exit(1);
    }
    close(fd);
    printf("write test OK\n");
}

void writebig()
{
    int i, fd, n;
    int MAXFILE = 2048;
    printf("==========writebig test==========\n");
    fd = open("big", O_CREATE|O_RDWR);
    if(fd < 0){
        printf("error: creat big failed!\n");
        exit(1);
    }

    for(i = 0; i < MAXFILE; i++){
        ((int*)buf)[0] = i;
        if(write(fd, buf, BSIZE) != BSIZE){
            printf("error: write big file failed\n", i);
            exit(1);
        }
    }
    close(fd);

    fd = open("big", O_RDONLY);
    if(fd < 0){
        printf("error: open big failed!\n");
        exit(1);
    }
    n = 0;
    for(;;){
        i = read(fd, buf, BSIZE);
        if(i == 0){
            if(n == MAXFILE - 1){
                printf("read only %d blocks from big", n);
                exit(1);
            }
            break;
        } else if(i != BSIZE){
            printf("read failed %d\n", i);
            exit(1);
        }
        if(((int*)buf)[0] != n){
            printf("read content of block %d is %d\n", n, ((int*)buf)[0]);
            exit(1);
        }
        n++;
    }
    close(fd);
    if(unlink("big") < 0){
        printf("unlink big failed\n");
        exit(1);
    }
    printf("writebig test OK\n");
}

void openiputtest()
{
    int pid, xstatus;
    printf("==========openiputtest test==========\n");

    if(mkdir("oidir", 0666) < 0){
        printf("mkdir oidir failed\n");
        exit(1);
    }
    pid = fork();
    if(pid < 0){
        printf("fork failed\n");
        exit(1);
    }
    if(pid == 0){
        int fd = open("oidir", O_RDWR);
        if(fd >= 0){
            printf("open directory for write succeeded\n");
            exit(1);
        }
        exit(0);
    }

    sleep(1);
    if(unlink("oidir") != 0){
        printf("unlink failed\n");
        exit(1);
    }

    wait(&xstatus);
    printf("openiputtest test OK\n");
    exit(xstatus);
}

void truncate1()
{
    char buf[32];
    printf("==========truncate1 test==========\n");

    unlink("truncfile");
    int fd1 = open("truncfile", O_CREATE|O_WRONLY|O_TRUNC);
    write(fd1, "abcd", 4);
    close(fd1);

    int fd2 = open("truncfile", O_RDONLY);
    int n = read(fd2, buf, sizeof(buf));
    if(n != 4){
        printf("read %d bytes, wanted 4\n", n);
        exit(1);
    }

    fd1 = open("truncfile", O_WRONLY|O_TRUNC);
    int fd3 = open("truncfile", O_RDONLY);
    n = read(fd3, buf, sizeof(buf));

    printf("n : %d\n", n);
    printf("n : %s\n", buf);

    if(n != 0){
        printf("aaa fd3=%d\n", fd3);
        printf("read %d bytes, wanted 0\n", n);
        exit(1);
    }

    // n = read(fd2, buf, sizeof(buf));
    // if(n != 0){
    //     printf("bbb fd2=%d\n", fd2);
    //     printf("read %d bytes, wanted 0\n", n);
    //     exit(1);
    // }

    // write(fd1, "abcdef", 6);

    // n = read(fd3, buf, sizeof(buf));
    // if(n != 6){
    //     printf("read %d bytes, wanted 6\n", n);
    //     exit(1);
    // }

    // n = read(fd2, buf, sizeof(buf));
    // if(n != 2){
    //     printf("read %d bytes, wanted 2\n", n);
    //     exit(1);
    // }

    // unlink("truncfile");

    // close(fd1);
    // close(fd2);
    // close(fd3);

    printf("truncate1 test OK\n");
}



int main(void)
{
    forktest();
    forkfork();
    opentest();
    exitwait();
    twochildren();
    reparent2();
    writetest();
    writebig();
    openiputtest();
    // truncate1();

    exit(0);
    return 0;
}

