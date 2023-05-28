#define USER
#include "stddef.h"
#include "unistd.h"
#include "stdio.h"
#include "string.h"
#include "stdlib.h"


void sigint_handler(int sig);

void sigint_handler(int sig) {
    printf("Received SIGINT signal, exiting.\n");
    exit(0);
}

int main() {
    struct sigaction my_sig;
    my_sig.sa_handler = sigint_handler;
    rt_sigaction(SIGINT, &my_sig, NULL, sizeof(sigset_t));
    // 注册 SIGINT 信号处理器
    // if (signal(SIGINT, sigint_handler) == SIG_ERR) {
    //     printf("error\n");
    //     return 1;
    // }
    // // 不断循环等待信号
    // while (1) {
    //     printf("Waiting for SIGINT signal...\n");
    //     sleep(1);
    // }

    return 0;
}