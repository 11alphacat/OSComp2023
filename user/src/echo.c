#define USER
#include "stddef.h"
#include "unistd.h"
#include "stdio.h"
#include "string.h"

int main(int argc, char *argv[])
{
  int i;
  for(i = 1; i < argc; i++){
    write(1, argv[i], strlen(argv[i]));
    if(i + 1 < argc){
      write(1, " ", 1);
    } else {
      write(1, "\n", 1);
    }
  }
  exit(0);
  return 0;
}
