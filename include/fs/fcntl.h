#ifndef __FCNTL_H__
#define __FCNTL_H__

// f_flags
#define O_RDONLY 0x000
#define O_WRONLY 0x001
#define O_RDWR 0x002
#define O_CREATE 0x40
#define O_DIRECTORY 0x0200000
// #define O_CREATE 0x200      // for xv6
#define O_TRUNC 0x400
#define F_WRITEABLE(fp) ((fp)->f_flags > 0 ? 1 : 0)
#define F_READABLE(fp) (((fp)->f_flags & O_WRONLY) == O_WRONLY ? 0 : 1)

// f_mode
#define IMODE_READONLY 0x01
#define IMODE_NONE 0x00

#define PROT_NONE 0x0
#define PROT_READ 0x1
#define PROT_WRITE 0x2
#define PROT_EXEC 0x4

#define MAP_FILE 0
#define MAP_SHARED 0x01
#define MAP_PRIVATE 0x02

#define AT_FDCWD -100

#define F_WRITEABLE(fp) ((fp)->f_flags > 0 ? 1 : 0)
#define F_READABLE(fp) (((fp)->f_flags & O_WRONLY) == O_WRONLY ? 0 : 1)
#endif // __FCNTL_H__
