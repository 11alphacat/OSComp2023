#define USER
#include "param.h"
#include "types.h"
#include "fs/inode/stat.h"
#include "user.h"
#include "fs/inode/fs.h"
#include "fs/fcntl.h"
#include "syscall_gen/syscall_num.h"
#include "memory/memlayout.h"
#include "riscv.h"


void
forkforkfork(char *s)
{
  unlink("stopforking");

  int pid = fork();
  if(pid < 0){
    printf("%s: fork failed", s);
    exit(1);
  }
  if(pid == 0){
    while(1){
      int fd = open("stopforking", 0);
      if(fd >= 0){
        exit(0);
      }
      if(fork() < 0){
        close(open("stopforking", O_CREATE|O_RDWR));
      }
    }

    exit(0);
  }

  sleep(20); // two seconds
  close(open("stopforking", O_CREATE|O_RDWR));
  wait(0);
  sleep(10); // one second
}


int
main(void)
{
  forkforkfork("forkforkfork");
  exit(0);
}
