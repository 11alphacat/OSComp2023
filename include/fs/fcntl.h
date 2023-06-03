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

#define O_NONBLOCK 00004
#define O_APPEND 00010
#define FCNTLABLE(value) \
    (value == O_NONBLOCK || value == O_APPEND)

#define F_WRITEABLE(fp) ((fp)->f_flags > 0 ? 1 : 0)
#define F_READABLE(fp) (((fp)->f_flags & O_WRONLY) == O_WRONLY ? 0 : 1)

// f_mode
#define IMODE_READONLY 0x01
#define IMODE_NONE 0x00

#define PROT_NONE 0x0
#define PROT_READ 0x1
#define PROT_WRITE 0x2
#define PROT_EXEC 0x4

#define AT_FDCWD -100

#define F_WRITEABLE(fp) ((fp)->f_flags > 0 ? 1 : 0)
#define F_READABLE(fp) (((fp)->f_flags & O_WRONLY) == O_WRONLY ? 0 : 1)

// lseek
#define SEEK_SET 0 /* seek relative to beginning of file */
#define SEEK_CUR 1 /* seek relative to current file position */
#define SEEK_END 2 /* seek relative to end of file */

// fcntl
#define F_DUPFD 0 /* dup */
#define F_GETFD 1 /* get close_on_exec */
#define F_SETFD 2 /* set/clear close_on_exec */
#define F_GETFL 3 /* get file->f_flags */
#define F_SETFL 4 /* set file->f_flags */

// faccess
#define F_OK 0           /* test existance */
#define R_OK 4           /* test readable */
#define W_OK 2           /* test writable */
#define X_OK 1           /* test executable */
#define AT_EACCESS 0x100 /* 使用进程的有效用户ID 和 组ID */
#endif                   // __FCNTL_H__
