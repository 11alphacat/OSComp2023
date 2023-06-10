// init: The initial user-level program
#define USER
#include "stddef.h"
#include "unistd.h"
#include "stdio.h"
#include "string.h"

char *argv[] = { "/init/sh", 0};
char *envp[] = {"PATH=/oscomp:/bin:/test:/busybox", 0};

#define CONSOLE 1
int
main(void)
{
  int pid, wpid;

  // if(open("console", O_RDWR) < 0){
  //   // mknod("console", CONSOLE, 0);
  //   open("console", O_RDWR);
  // }
  
  // if(openat(AT_FDCWD,"/dev/console.dev", O_RDWR) < 0){
  //   mknod("/dev/console.dev", CONSOLE,0);
  //   openat(AT_FDCWD,"/dev/console.dev", O_RDWR);
  // }
  if(openat(AT_FDCWD,"/dev/tty", O_RDWR) < 0){
    mknod("/dev/tty", CONSOLE,0);
    openat(AT_FDCWD,"/dev/tty", O_RDWR);
  }

  // printf("\n\tNow you come here, welcome my dear friend.\n\n");
  dup(0);  // stdout
  dup(0);  // stderr

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
      execve("/bin/sh",argv,envp);
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
