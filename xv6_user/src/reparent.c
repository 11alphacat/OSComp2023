#define USER
#include "stddef.h"
#include "unistd.h"
#include "stdio.h"
#include "string.h"


// try to find races in the reparenting
// code that handles a parent exiting
// when it still has live children.
void
reparent(char *s)
{
  int master_pid = getpid();
  printf("hit");
  for(int i = 0; i < 200; i++){
    int pid = fork();
    if(pid < 0){
      printf("%s: fork failed\n", s);
      exit(1);
    }
    if(pid){
      if(wait(0) != pid){
        printf("%s: wait wrong pid\n", s);
        exit(1);
      }
    } else {
      int pid2 = fork();
      if(pid2 < 0){
        kill(master_pid);
        exit(1);
      }
      exit(0);
    }
  }
  printf("reparent test OK\n");
  exit(0);
}


int
main(void)
{
  reparent("reparent");
  exit(0);
  return 0;
}
