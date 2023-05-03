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

// regression test. does reparent() violate the parent-then-child
// locking order when giving away a child to init, so that exit()
// deadlocks against init's wait()? also used to trigger a "panic:
// release" due to exit() releasing a different p->parent->lock than
// it acquired.
void
reparent2(char *s)
{
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

  exit(0);
}

int
main(void)
{
  reparent2("reparent2");
  exit(0);
}