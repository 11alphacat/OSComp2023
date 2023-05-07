#define USER
#include "types.h"
#include "user.h"
#include "fs/stat.h"
#include "fs/fcntl.h"

int
main(int argc, char *argv[])
{
  int i;

  if(argc != 2){
    fprintf(2, "Usage: mkdir file\n");
    exit(1);
  }

  for(i = 1; i < argc; i++){
    if(mkdirat(AT_FDCWD, argv[i], 0666) < 0){
      fprintf(2, "mkdirat: %s failed to create\n", argv[i]);
      break;
    }
  }

  exit(0);
}
