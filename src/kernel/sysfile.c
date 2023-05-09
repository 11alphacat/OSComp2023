//
// File-system system calls.
// this sysfile implements oscomp fs syscalls based on fat32 interface
// Mostly argument checking, since we don't trust
// user code, and calls into fat32_file.c and fat32_inode.c.
//

#include "common.h"
#include "riscv.h"
#include "param.h"
#include "debug.h"
#include "atomic/spinlock.h"
#include "proc/pcb_life.h"

#include "fs/fcntl.h"
#include "kernel/trap.h"
#include "debug.h"
#include "memory/allocator.h"
#include "proc/pipe.h"
#include "memory/vm.h"
#include "proc/exec.h"
#include "fs/fat/fat32_mem.h"
#include "fs/fat/fat32_file.h"
#include "fs/vfs/fs.h"
#include "fs/stat.h"

// Fetch the nth word-sized system call argument as a file descriptor
// and return both the descriptor and the corresponding struct file.
// 它是线程安全的
int argfd(int n, int *pfd, struct _file **pf) {
    int fd;
    struct _file *f;
    struct proc *p = current();
    argint(n, &fd);
    if (fd < 0 || fd >= NOFILE)
        return -1;

    // in case another thread writes after the current thead reads
    acquire(&p->tlock);
    if ((f = current()->_ofile[fd]) == 0) {
        release(&p->tlock);
        return -1;
    } else {
        if (pfd)
            *pfd = fd;
        if (pf)
            *pf = f;
    }
    release(&p->tlock);
    return 0;
}

static inline int __namecmp(const char *s, const char *t) {
    return strncmp(s, t, PATH_LONG_MAX);
}

// Allocate a file descriptor for the given file.
// Takes over file reference from caller on success.
// 它是线程安全的
static int fdalloc(struct _file *f) {
    int fd;
    struct proc *p = current();

    acquire(&p->tlock);
    for (fd = 0; fd < NOFILE; fd++) {
        if (p->_ofile[fd] == 0) {
            p->_ofile[fd] = f;
            release(&p->tlock);
            return fd;
        }
    }
    release(&p->tlock);
    return -1;
}

// 如果path是相对路径，则它是相对于dirfd目录而言的。
// 如果path是相对路径，且dirfd的值为AT_FDCWD，则它是相对于当前路径而言的。
// 如果path是绝对路径，则dirfd被忽略。
// 一般不对 path作检查
// 如果name字段不为null，返回的是父目录的inode节点，并填充name字段
static struct _inode *find_inode(char *path, int dirfd, char *name) {
    ASSERT(path);
    struct _inode *ip;
    struct proc *p = current();
    if (*path == '/' || dirfd == AT_FDCWD) {
        // 绝对路径 || 相对于当前路径，忽略 dirfd
        // acquire(&p->tlock);
        ip = (!name) ? fat32_name_inode(path) : fat32_name_inode_parent(path, name);
        if (ip == 0) {
            // release(&p->tlock);
            return 0;
        } else {
            // release(&p->tlock);
            return ip;
        }
    } else {
        // path为相对于 dirfd目录的路径
        struct _file *f;
        // acquire(&p->tlock);
        if (dirfd < 0 || dirfd >= NOFILE || (f = p->_ofile[dirfd]) == 0) {
            // release(&p->tlock);
            return 0;
        }
        struct _inode *oldcwd = p->_cwd;
        p->_cwd = f->f_tp.f_inode;
        ip = (!name) ? fat32_name_inode(path) : fat32_name_inode_parent(path, name);
        if (ip == 0) {
            // release(&p->tlock);
            return 0;
        }
        p->_cwd = oldcwd;
        // release(&p->tlock);
    }
    return ip;
}

static struct _inode *inode_create(char *path, int dirfd, uchar type) {
    ASSERT(type > 0 && type < 4);
    // TODO!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!
    struct _inode *ip = NULL, *dp = NULL;
    char name[NAME_LONG_MAX];

    // without parent fat_entry
    if ((dp = find_inode(path, dirfd, name)) == 0)
        return 0;
    fat32_inode_lock(dp);

    // have existed?
    if ((ip = fat32_inode_dirlookup(dp, name, 0)) != 0) {
        fat32_inode_unlock_put(dp);
        fat32_inode_lock(ip);
        // if (type == T_FILE && (ip->i_type == T_FILE || ip->i_type == T_DEVICE))
        if (type == ip->i_type)
            return ip;
        fat32_inode_unlock_put(ip);
        return 0;
        // return 0;
    }

    // haven't exited
    if ((ip = fat32_inode_alloc(dp, name, type)) == 0) {
        fat32_inode_unlock_put(dp);
        return 0;
    }

    fat32_inode_lock(ip);
    // ip->i_nlink = 1;
    fat32_inode_update(ip);

    if (type == T_DIR) { // Create . and .. entries.
        // // No ip->nlink++ for ".": avoid cyclic ref count.
        // if (fat32_inode_dirlink(ip, ".") < 0 || fat32_inode_dirlink(ip, "..") < 0)
        //     goto fail;
        // TODO : dirlink

        // direntory . and .. , write them to the disk
        char fcb_dot_char[64];
        memset(fcb_dot_char, 0, sizeof(fcb_dot_char));
        fat32_fcb_init(ip, (uchar *)".", ATTR_DIRECTORY, fcb_dot_char);
        uint tot = fat32_inode_write(ip, 0, (uint64)fcb_dot_char, 0, 32);
        ASSERT(tot == 32);

        char fcb_dotdot_char[64];
        memset(fcb_dotdot_char, 0, sizeof(fcb_dotdot_char));
        fat32_fcb_init(ip, (uchar *)"..", ATTR_DIRECTORY, fcb_dotdot_char);
        tot = fat32_inode_write(ip, 0, (uint64)fcb_dotdot_char, 32, 32);
        ASSERT(tot == 32);
    }

    // if (fat32_inode_dirlink(dp, name) < 0)
    //     goto fail;

    if (type == T_DIR) {
        // now that success is guaranteed:
        fat32_inode_update(dp);
    }

    fat32_inode_unlock_put(dp);
    return ip;

    // fail:
    //     ip->i_nlink = 0;
    //     fat32_inode_update(ip);
    //     fat32_inode_unlock_put(ip);
    //     fat32_inode_unlock_put(dp);
    //     return 0;
}

uint64 sys_mknod(void) {
    struct _inode *ip;
    char path[MAXPATH];
    int major, minor;

    argint(1, &major);
    argint(2, &minor);

    // TODO() handle major and minor

    if ((argstr(0, path, MAXPATH)) < 0 || (ip = fat32_inode_create(path, T_DEVICE, major, minor)) == 0) {
        return -1;
    }
    fat32_inode_unlock_put(ip);
    return 0;
}

// 功能：获取当前工作目录；
// 输入：
// - char *buf：一块缓存区，用于保存当前工作目录的字符串。当buf设为NULL，由系统来分配缓存区。
// - size：buf缓存区的大小。
// 返回值：成功执行，则返回当前工作目录的字符串的指针。失败，则返回NULL。
//  The  getcwd()  function  copies  an  absolute  pathname
// of the current working directory to the array pointed to by buf,
// which is of length size.
// Maybe we need dirent ?
uint64 sys_getcwd(void) {
    uint64 buf;
    size_t size;
    struct proc *p = current();
    argaddr(0, &buf);
    argulong(1, &size);

    char kbuf[PATH_LONG_MAX];
    memset(kbuf, 0, PATH_LONG_MAX); // important! init before use!
    fat32_getcwd(kbuf);
    if (!buf && (buf = (uint64)kalloc()) == 0) {
        return (uint64)NULL;
    }

    if (copyout(p->pagetable, buf, kbuf, strnlen(kbuf, PATH_LONG_MAX) + 1) < 0) {   // rember add 1 for '\0'
        return (uint64)NULL;
    } else {
        return buf;
    }
}

// 功能：复制文件描述符；
// 输入：
// - fd：被复制的文件描述符。
// 返回值：成功执行，返回新的文件描述符。失败，返回-1。
uint64 sys_dup(void) {
    struct _file *f;
    int fd;

    if (argfd(0, 0, &f) < 0)
        return -1;
    if ((fd = fdalloc(f)) < 0)
        return -1;
    fat32_filedup(f);
    return fd;
}

// 功能：复制文件描述符，并指定了新的文件描述符；
// 输入：
// - old：被复制的文件描述符。
// - new：新的文件描述符。
// 返回值：成功执行，返回新的文件描述符。失败，返回-1。
// Mention:
//    If the file descriptor newfd was previously open, it is silently closed before being reused.
//    The steps of closing and reusing the file descriptor newfd are performed  atomically.
uint64 sys_dup3(void) {
    struct _file *f;
    struct proc *p = current();
    int oldfd, newfd, flags;

    if (argfd(0, &oldfd, &f) < 0) {
        return -1;
    }
    argint(1, &newfd);
    if (newfd < 0 || newfd >= NOFILE) {
        return -1;
    }
    argint(2, &flags);
    ASSERT(flags == 0);
    if (oldfd == newfd) {
        // do nothing
        return newfd;
    }

    acquire(&p->tlock); // 可以修改为粒度小一些的锁;可以往_file结构里加锁，或者fdtable
    if (p->_ofile[newfd] == 0) {
        // not used, great!
        p->_ofile[newfd] = f;
        release(&p->tlock);
    } else {
        // close and reuse
        // two steps must be atomic!
        fat32_fileclose(p->_ofile[newfd]);
        p->_ofile[newfd] = f;
        release(&p->tlock);
    }

    fat32_filedup(f);
    return newfd;
}

// * 功能：挂载文件系统；
// * 输入：
//   - special: 挂载设备；
//   - dir: 挂载点；
//   - fstype: 挂载的文件系统类型；
//   - flags: 挂载参数；
//   - data: 传递给文件系统的字符串参数，可为NULL；
// * 返回值：成功返回0，失败返回-1；
// uint64 sys_mount(void) {
//     return 0;
// }

// 功能：打开或创建一个文件；
// 输入：
// - fd：文件所在目录的文件描述符。
// - filename：要打开或创建的文件名。如为绝对路径，则忽略fd。如为相对路径，且fd是AT_FDCWD，则filename是相对于当前工作目录来说的。如为相对路径，且fd是一个文件描述符，则filename是相对于fd所指向的目录来说的。
// - flags：必须包含如下访问模式的其中一种：O_RDONLY，O_WRONLY，O_RDWR。还可以包含文件创建标志和文件状态标志。
// - mode：文件的所有权描述。详见`man 7 inode `。
// 返回值：成功执行，返回新的文件描述符。失败，返回-1。
uint64 sys_openat(void) {
    /*
        The openat() system call operates in exactly the same way as open(), except for the differences described here.

        If the pathname given in pathname is relative, then it is interpreted relative to the directory referred to by the file  de‐
        scriptor  dirfd  (rather  than  relative to the current working directory of the calling process, as is done by open() for a
        relative pathname).

        If pathname is relative and dirfd is the special value AT_FDCWD, then pathname is interpreted relative to the current  work‐
        ing directory of the calling process (like open()).

        If pathname is absolute, then dirfd is ignored.
    */

    char path[PATH_LONG_MAX];
    int dirfd, flags, omode;
    struct _file *f;
    struct _inode *ip;
    int n;
    argint(0, &dirfd); // no need to check dirfd, because dirfd maybe AT_FDCWD(<0)
                       // find_inode will do the check
    if ((n = argstr(1, path, PATH_LONG_MAX)) < 0)
        return -1;
    argint(2, &flags);
    argint(3, &omode);

    // 如果是要求创建文件，则调用 create
    if ((flags & O_CREATE) == O_CREATE) {
        if ((ip = inode_create(path, dirfd, T_FILE)) == 0) {
            return -1;
        }
    } else {
        // 否则，我们先调用 find_inode找到path对应的文件inode节点
        if ((ip = find_inode(path, dirfd, 0)) == 0) {
            return -1;
        }
        ASSERT(ip); // 这里 ip 应该已绑定到 path 找到的文件inode节点
        fat32_inode_lock(ip);
        // if (ip->i_type == T_DIR && flags != O_RDONLY) {
        //     fat32_inode_unlock_put(ip);
        //     return -1;
        // }
        if ((flags & O_DIRECTORY) && ip->i_type != T_DIR) {
            fat32_inode_unlock_put(ip);
            return -1;
        }
    }

    if (ip->i_type == T_DEVICE && (ip->i_dev < 0 || ip->i_dev >= NDEV)) {
        fat32_inode_unlock_put(ip);
        return -1;
    }

    // 下面为inode文件分配一个打开文件表项，为进程分配一个文件描述符
    int fd;
    if ((f = fat32_filealloc()) == 0 || (fd = fdalloc(f)) < 0) {
        if (f)
            fat32_fileclose(f);
        fat32_inode_unlock_put(ip);
        return -1;
    }

    // 下面为 f 结构体填充字段
    if (ip->i_type == T_DEVICE) {
        f->f_type = FD_DEVICE;
        f->f_major = ip->i_dev;
    } else {
        f->f_type = FD_INODE;
        f->f_pos = 0;
    }
    f->f_tp.f_inode = ip;
    f->f_flags = flags; // TODO(): &
    f->f_mode = omode;  // TODO(): &
    f->f_count = 1;
    /*  for VFS
    f_owner = TODO();
    f_op = TODO();
    f_version = TODO();
    */

    if ((omode & O_TRUNC) && ip->i_type == T_FILE) {
        fat32_inode_trunc(ip);
    }

    fat32_inode_unlock(ip);

    return fd;
}

// 功能：关闭一个文件描述符；
// 输入：
// - fd：要关闭的文件描述符。
// 返回值：成功执行，返回0。失败，返回-1。
uint64 sys_close(void) {
    int fd;
    struct _file *f;

    if (argfd(0, &fd, &f) < 0)
        return -1;
    current()->_ofile[fd] = 0;
    fat32_fileclose(f);
    return 0;
}

// 功能：从一个文件描述符中读取；
// 输入：
// - fd：要读取文件的文件描述符。
// - buf：一个缓存区，用于存放读取的内容。
// - count：要读取的字节数。
// 返回值：成功执行，返回读取的字节数。如为0，表示文件结束。错误，则返回-1。
uint64 sys_read(void) {
    struct _file *f;
    int n;
    uint64 p;

    argaddr(1, &p);
    argint(2, &n);
    if (argfd(0, 0, &f) < 0)
        return -1;
    return fat32_fileread(f, p, n);
}

// 功能：从一个文件描述符中写入；
// 输入：
// - fd：要写入文件的文件描述符。
// - buf：一个缓存区，用于存放要写入的内容。
// - count：要写入的字节数。
// 返回值：成功执行，返回写入的字节数。错误，则返回-1。
uint64 sys_write(void) {
    struct _file *f;
    int n;
    uint64 p;

    argaddr(1, &p);
    argint(2, &n);
    if (argfd(0, 0, &f) < 0)
        return -1;

    return fat32_filewrite(f, p, n);
}

// 功能：创建文件的链接；
// 输入：
// - olddirfd：原来的文件所在目录的文件描述符。
// - oldpath：文件原来的名字。如果oldpath是相对路径，则它是相对于olddirfd目录而言的。如果oldpath是相对路径，且olddirfd的值为AT_FDCWD，则它是相对于当前路径而言的。如果oldpath是绝对路径，则olddirfd被忽略。
// - newdirfd：新文件名所在的目录。
// - newpath：文件的新名字。newpath的使用规则同oldpath。
// - flags：在2.6.18内核之前，应置为0。其它的值详见`man 2 linkat`。
// 返回值：成功执行，返回0。失败，返回-1。
uint64 sys_linkat(void) {
    char newpath[MAXPATH], oldpath[MAXPATH];
    int olddirfd, newdirfd, flags;

    if (argstr(1, oldpath, MAXPATH) < 0 || argstr(3, newpath, MAXPATH) < 0) {
        return -1;
    }
    argint(0, &olddirfd);
    argint(2, &newdirfd);
    argint(4, &flags);
    ASSERT(flags == 0);

    char oldname[MAXPATH], newname[MAXPATH];
    struct _inode *oldip, *newdp;
    if ((newdp = find_inode(newpath, newdirfd, newname)) == 0) {
        // 新路径的目录不存在
        return -1;
    }
    if ((oldip = find_inode(oldpath, olddirfd, oldname)) == 0) {
        // 旧文件的inode不存在
        return -1;
    }
    if (fat32_inode_dirlookup(newdp, newname, 0) != 0) {
        // 新文件已存在
        return 0;
    }

    // 往 newdp 中 写入一个代表 oldip 的项

    // TODO()
    // if ( fat32_dirlink(olddp, newdp) < 0) {
    //     return -1;
    // }

    return 0;
}

// 功能：移除指定文件的链接(可用于删除文件)；
// 输入：
// - dirfd：要删除的链接所在的目录。
// - path：要删除的链接的名字。如果path是相对路径，则它是相对于dirfd目录而言的。如果path是相对路径，且dirfd的值为AT_FDCWD，则它是相对于当前路径而言的。如果path是绝对路径，则dirfd被忽略。
// - flags：可设置为0或AT_REMOVEDIR。
// 返回值：成功执行，返回0。失败，返回-1。
uint64 sys_unlinkat(void) {
    struct _inode *ip, *dp;
    // struct dirent de;
    char name[NAME_LONG_MAX], path[PATH_LONG_MAX];
    int dirfd, flags;
    uint off;
    argint(0, &dirfd); // don't need to check, because find_inode will do
    argint(2, &flags);
    ASSERT(flags == 0);
    if (argstr(1, path, PATH_LONG_MAX) < 0)
        return -1;

    if ((dp = find_inode(path, dirfd, name)) == 0) {
        return -1;
    }

    fat32_inode_lock(dp);
    // Cannot unlink "." or "..".
    if (__namecmp(name, ".") == 0 || __namecmp(name, "..") == 0)
        goto bad;

    if ((ip = fat32_inode_dirlookup(dp, name, &off)) == 0)
        goto bad;

    fat32_inode_lock(ip);
    if (ip->i_nlink < 1)
        panic("unlink: nlink < 1");
    if (ip->i_type == T_DIR && !fat32_isdirempty(ip)) {
        // 试图删除的是目录文件，并且非空
        fat32_inode_unlock_put(ip);
        goto bad;
    }

    // 处理父目录区
    // if (fat32_inode_write(dp, 0, (uint64)&de, off, sizeof(de)) != sizeof(de))
    //     panic("unlink: writei");

    fat32_inode_delete(dp, ip);

    if (ip->i_type == T_DIR) {
        // 试图删除的是空的目录文件
        dp->i_nlink--;
        fat32_inode_update(dp);
    }
    fat32_inode_unlock_put(dp);

    ip->i_nlink--;
    fat32_inode_update(ip);
    fat32_inode_unlock_put(ip);

    return 0;

bad:
    fat32_inode_unlock_put(dp);
    return -1;
}

// 功能：创建目录；
// 输入：
// - dirfd：要创建的目录所在的目录的文件描述符。
// - path：要创建的目录的名称。如果path是相对路径，则它是相对于dirfd目录而言的。如果path是相对路径，且dirfd的值为AT_FDCWD，则它是相对于当前路径而言的。如果path是绝对路径，则dirfd被忽略。
// - mode：文件的所有权描述。详见`man 7 inode `。
// 返回值：成功执行，返回0。失败，返回-1。
uint64 sys_mkdirat(void) {
    char path[PATH_LONG_MAX];
    int dirfd;
    mode_t mode;
    struct _inode *ip;
    argint(0, &dirfd);
    if (argint(2, (int *)&mode) < 0)
        return -1;
    if (argstr(1, path, PATH_LONG_MAX) < 0 || (ip = inode_create(path, dirfd, T_DIR)) == 0) {
        return -1;
    }
    ip->i_mode = mode;
    fat32_inode_unlock_put(ip);
    return 0;
}

/*
struct dirent {
    uint64 d_ino;	// 索引结点号
    int64 d_off;	// 到下一个dirent的偏移
    unsigned short d_reclen;	// 当前dirent的长度
    unsigned char d_type;	// 文件类型
    char d_name[];	//文件名
};
*/

// 功能：获取目录的条目;
// 输入：
// - fd：所要读取目录的文件描述符。
// - buf：一个缓存区，用于保存所读取目录的信息。
// - len：buf的大小。
// 返回值：成功执行，返回读取的字节数。当到目录结尾，则返回0。失败，则返回-1。
uint64 sys_getdents64(void) {
    struct _file *f;
    uint64 buf; // user pointer to struct dirent
    int len;
    ssize_t nread, sz;
    char* kbuf;
    // const size_t MAXLEN = 2048;

    if (argfd(0, 0, &f) < 0)
        return -1;

    // if (f->f_type != T_DIR) {
    //     return -1;
    // }
    if (f->f_type != FD_INODE) {
        return -1;
    }

    if (f->f_pos == f->f_tp.f_inode->i_size) {
        return 0;
    }
    argaddr(1, &buf);
    argint(2, &len);
    if (len <= 0) {
        return -1;
    }
    sz = f->f_tp.f_inode->fat32_i.cluster_cnt * f->f_tp.f_inode->i_sb->cluster_size;
    // ASSERT(sz > len);
    if ( (kbuf = kmalloc(sz)) == 0 ) {
        return -1;
    }
    memset(kbuf, 0, len); // !!!!
    if ( ( nread = fat32_getdents(f->f_tp.f_inode, kbuf, sz) ) < 0 ) {
        kfree(kbuf);
        return -1;
    }
    len = len > sz ? sz : len;
    if (either_copyout(1, buf, kbuf, len) < 0) {    // copy lenth may less than nread
        kfree(kbuf);
        return -1;
    }
    kfree(kbuf);
    f->f_pos = f->f_tp.f_inode->i_size;
    return nread;
}

// 功能：获取文件状态；
// 输入：
// - fd: 文件句柄；
// - kst: 接收保存文件状态的指针；
// 返回值：成功返回0，失败返回-1；
uint64 sys_fstat(void) {
    struct _file *f;
    uint64 st; // user pointer to struct kstat

    argaddr(1, &st);
    if (argfd(0, 0, &f) < 0)
        return -1;
    return fat32_filestat(f, st);
}

// 功能：切换工作目录；
// 输入：
// - path：需要切换到的目录。
// 返回值：成功执行，返回0。失败，返回-1。
uint64 sys_chdir(void) {
    char path[PATH_LONG_MAX];
    struct _inode *ip;
    struct proc *p = current();

    // begin_op();
    if (argstr(0, path, PATH_LONG_MAX) < 0 || (ip = fat32_name_inode(path)) == 0) { // bug: 修改了n_link
        // end_op();
        return -1;
    }

    fat32_inode_lock(ip); // bug:修改了 i_mode
    if (ip->i_type != T_DIR) {
        fat32_inode_unlock_put(ip);
        // end_op();
        return -1;
    }

    fat32_inode_unlock(ip);
    // fat32_inode_put(p->_cwd);
    p->_cwd = ip;
    return 0;
}

// 功能：创建管道；
// 输入：
// - fd[2]：用于保存2个文件描述符。其中，fd[0]为管道的读出端，fd[1]为管道的写入端。
// 返回值：成功执行，返回0。失败，返回-1。
uint64 sys_pipe2(void) {
    uint64 fdarray; // user pointer to array of two integers
    struct _file *rf, *wf;
    int fd0, fd1;
    struct proc *p = current();

    argaddr(0, &fdarray);
    if (_pipealloc(&rf, &wf) < 0) // 分配两个 pipe 文件
        return -1;
    fd0 = -1;
    if ((fd0 = fdalloc(rf)) < 0 || (fd1 = fdalloc(wf)) < 0) { // 给当前进程分配两个文件描述符，代指那两个管道文件
        if (fd0 >= 0)
            p->_ofile[fd0] = 0;
        fat32_fileclose(rf);
        fat32_fileclose(wf);
        return -1;
    }
    if (copyout(p->pagetable, fdarray, (char *)&fd0, sizeof(fd0)) < 0
        || copyout(p->pagetable, fdarray + sizeof(fd0), (char *)&fd1, sizeof(fd1)) < 0) {
        p->_ofile[fd0] = 0;
        p->_ofile[fd1] = 0;
        fat32_fileclose(rf);
        fat32_fileclose(wf);
        return -1;
    }
    return 0;
}

// temporary version
uint64 sys_execve(void) {
    char path[MAXPATH], *argv[MAXARG];
    int i;
    uint64 uargv, uarg;

    argaddr(1, &uargv);
    if (argstr(0, path, MAXPATH) < 0) {
        return -1;
    }
    memset(argv, 0, sizeof(argv));
    for (i = 0;; i++) {
        if (i >= NELEM(argv)) {
            goto bad;
        }
        if (fetchaddr(uargv + sizeof(uint64) * i, (uint64 *)&uarg) < 0) {
            goto bad;
        }
        if (uarg == 0) {
            argv[i] = 0;
            break;
        }
        argv[i] = kalloc();
        if (argv[i] == 0)
            goto bad;
        if (fetchstr(uarg, argv[i], PGSIZE) < 0)
            goto bad;
    }

    int ret = do_execve(path, argv, NULL);

    for (i = 0; i < NELEM(argv) && argv[i] != 0; i++)
        kfree(argv[i]);

    return ret;

bad:
    for (i = 0; i < NELEM(argv) && argv[i] != 0; i++)
        kfree(argv[i]);
    return -1;
}