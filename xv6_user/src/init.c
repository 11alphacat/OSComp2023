// init: The initial user-level program
#define USER
#include "types.h"
#include "fs/stat.h"
#include "user.h"
#include "fs/fcntl.h"

char *argv[] = { "sh", 0 };

int
main(void)
{
  int pid, wpid;

  // if(open("console", O_RDWR) < 0){
  //   // mknod("console", CONSOLE, 0);
  //   open("console", O_RDWR);
  // }
  
  if(openat(AT_FDCWD,"console.dev", O_RDWR) < 0){
    mknod("console.dev", CONSOLE,0);
    openat(AT_FDCWD,"console.dev", O_RDWR);
  }

  dup(0);  // stdout
  dup(0);  // stderr

  printf("\n\tNow you come here, welcome my dear friend.\n\n");

  for (;;) {
    printf("init: starting sh\n");
    
    pid = fork();
    if (pid < 0) {
      printf("init: fork failed\n");
      exit(1);
    }
    if (pid == 0) {
      printf("about to start sh\n");
      execve("sh",argv,0);
      printf("init: exec sh failed\n");
      exit(1);
    }
    
    
    for(;;){
      // this call to wait() returns if the shell exits,
      // or if a parentless process exits.
      wpid = wait((int *) 0);
      if (wpid == pid) {
        // the shell exited; restart it.
        break;
      } 
      else if (wpid < 0){
        printf("init: wait returned an error\n");
        exit(1);
      } 
      else {
        // it was a parentless process; do nothing.
      }
    }
  }
}
