#define USER
#include "types.h"
#include "fs/inode/stat.h"
#include "user.h"

#define N  1000

void
print(const char *s)
{
  write(1, s, strlen(s));
}


// what if two children exit() at the same time?
void
twochildren(char *s)
{
  for(int i = 0; i < 400; i++){
    int pid1 = fork();
    if(pid1 < 0){
      printf("%s: fork failed\n", s);
      exit(1);
    }
    if(pid1 == 0){
      exit(0);
    } else {
      int pid2 = fork();
      if(pid2 < 0){
        printf("%s: fork failed\n", s);
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
}

int
main(void)
{
  twochildren("twochildren");
  exit(0);
}
