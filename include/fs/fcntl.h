#ifndef __FCNTL_H__
#define __FCNTL_H__

// f_flags
#define O_RDONLY 0x000
#define O_WRONLY 0x001
#define O_RDWR 0x002
#define O_CREATE 0x40
#define O_DIRECTORY 0x0200000
// #define O_CREAT 0x200
// #define O_CREATE 0x200      // for xv6
#define O_TRUNC 0x400

#define AT_FDCWD -100




#endif // __FCNTL_H__
