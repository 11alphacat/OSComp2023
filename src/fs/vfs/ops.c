#include "debug.h"
#include "proc/pipe.h"
#include "proc/pcb_life.h"
#include "fs/fcntl.h"
#include "fs/vfs/fs.h"
#include "fs/vfs/ops.h"
#include "fs/fat/fat32_file.h"
#include "fs/fat/fat32_mem.h"
#include "fs/ext2/ext2_file.h"

struct devsw devsw[NDEV];
struct ftable _ftable;

// == file layrer ==
struct file *filealloc(void) {
// Allocate a file structure.
// 语义：从内存中的 _ftable 中寻找一个空闲的 file 项，并返回指向该 file 的指针
    struct file *f;

    acquire(&_ftable.lock);
    for (f = _ftable.file; f < _ftable.file + NFILE; f++) {
        if (f->f_count == 0) {
            f->f_count = 1;

            // f->f_op = get_fat32_fileops();
            ASSERT(current()->_cwd->fs_type==FAT32);
            f->f_op = get_fileops[current()->_cwd->fs_type]();
            
            release(&_ftable.lock);
            return f;
        }
    }
    release(&_ftable.lock);
    return 0;
}

void generic_fileclose(struct file *f) {
// Close file f.  (Decrement ref count, close when reaches 0.)
// 语义：自减 f 指向的file的引用次数，如果为0，则关闭
// 对于管道文件，调用pipeclose
// 否则，调用iput归还inode节点
    struct file ff;

    acquire(&_ftable.lock);
    if (f->f_count < 1)
        panic("generic_fileclose");
    if (--f->f_count > 0) {
        release(&_ftable.lock);
        return;
    }
    ff = *f;
    f->f_count = 0;
    f->f_type = FD_NONE;
    release(&_ftable.lock);

    if (ff.f_type == FD_PIPE) {
        int wrable = F_WRITEABLE(&ff);
        pipeclose(ff.f_tp.f_pipe, wrable);
    } else if (ff.f_type == FD_INODE || ff.f_type == FD_DEVICE) {
        // begin_op();
        // fat32_inode_put(ff.f_tp.f_inode);
        ff.f_tp.f_inode->i_op->iput(ff.f_tp.f_inode);
        // end_op();
    }
}

static inline const struct file_operations* get_fat32_fileops(void) {
    // Mayer's singleton
    static const struct file_operations fops_instance = {
        .dup = fat32_filedup,
        .read = fat32_fileread,
        .write = fat32_filewrite,
        .fstat = fat32_filestat,
    };

    return &fops_instance;
}

static inline const struct file_operations* get_ext2_fileops(void) {
    ASSERT(0);
    return NULL;
}

        // Not to be moved upward 
const struct file_operations* (*get_fileops[])(void) = {
    [FAT32] get_fat32_fileops,
    [EXT2] get_ext2_fileops,
};



// == inode layer ==
static char *skepelem(char *path, char *name);
static struct inode *inode_namex(char *path, int nameeparent, char *name);

struct inode *find_inode(char *path, int dirfd, char *name) {
// 如果path是相对路径，则它是相对于dirfd目录而言的。
// 如果path是相对路径，且dirfd的值为AT_FDCWD，则它是相对于当前路径而言的。
// 如果path是绝对路径，则dirfd被忽略。
// 一般不对 path作检查
// 如果name字段不为null，返回的是父目录的inode节点，并填充name字段
    ASSERT(path);
    struct inode *ip;
    struct proc *p = current();
    if (*path == '/' || dirfd == AT_FDCWD) {
        // 绝对路径 || 相对于当前路径，忽略 dirfd
        // acquire(&p->tlock);
        ip = (!name) ? namei(path) : namei_parent(path, name);
        if (ip == 0) {
            // release(&p->tlock);
            return 0;
        } else {
            // release(&p->tlock);
            return ip;
        }
    } 
    else {
        // path为相对于 dirfd目录的路径
        struct file *f;
        // acquire(&p->tlock);
        if (dirfd < 0 || dirfd >= NOFILE || (f = p->_ofile[dirfd]) == 0) {
            // release(&p->tlock);
            return 0;
        }
        struct inode *oldcwd = p->_cwd;
        p->_cwd = f->f_tp.f_inode;
        ip = (!name) ? namei(path) : namei_parent(path, name);
        if (ip == 0) {
            // release(&p->tlock);
            return 0;
        }
        p->_cwd = oldcwd;
        // release(&p->tlock);
    }

    return ip;
}

static char *skepelem(char *path, char *name) {
// Examples:
//   skepelem("a/bb/c", name) = "bb/c", setting name = "a"
//   skepelem("///a//bb", name) = "bb", setting name = "a"
//   skepelem("a", name) = "", setting name = "a"
//   skepelem("", name) = skepelem("////", name) = 0

//   skepelem("./mnt", name) = "", setting name = "mnt"
//   skepelem("../mnt", name) = "", setting name = "mnt"
//   skepelem("..", name) = "", setting name = 0
    char *s;
    int len;

    while (*path == '/' || *path == '.')
        path++;
    if (*path == 0)
        return 0;
    s = path;
    while (*path != '/' && *path != 0)
        path++;
    len = path - s;
    if (len >= PATH_LONG_MAX)
        memmove(name, s, PATH_LONG_MAX);
    else {
        memmove(name, s, len);
        name[len] = 0;
    }
    while (*path == '/')
        path++;
    return path;
}

static struct inode *inode_namex(char *path, int nameeparent, char *name) {
    struct inode *ip = NULL, *next = NULL, *cwd = current()->_cwd;
    ASSERT(cwd);
    if (*path == '/') {
        ASSERT(cwd->i_sb);
        ASSERT(cwd->i_sb->root);
        struct inode *rip = cwd->i_sb->root; 
        // ip = fat32_inode_dup(cwd->i_sb->root);
        ip = rip->i_op->idup(rip);
    }
    else if (strncmp(path, "..", 2) == 0) {
        ip = cwd->parent->i_op->idup(cwd->parent);
    } 
    else {
        // ip = fat32_inode_dup(cwd);
        ip = cwd->i_op->idup(cwd);
    }

    while ((path = skepelem(path, name)) != 0) {
        // fat32_inode_lock(ip);
        ip->i_op->ilock(ip);
        // not a directory?
        // if (!DIR_BOOL(ip->fat32_i.Attr)) {
        if (!ip->i_op->idir(ip)) {
            // fat32_inode_unlock_put(ip);
            ip->i_op->iunlock_put(ip);
            return 0;
        }
        if (nameeparent && *path == '\0') {
            // Stop one level early.
            // fat32_inode_unlock(ip);
            ip->i_op->iunlock(ip);
            return ip;
        }
        // if ((next = fat32_inode_dirlookup(ip, name, 0)) == 0) {
        if ((next = ip->i_op->idirlookup(ip,name,0) ) == 0) {
            // fat32_inode_unlock_put(ip);
            ip->i_op->iunlock_put(ip);
            return 0;
        }
        // fat32_inode_unlock_put(ip);
        ip->i_op->iunlock_put(ip);
        ip = next;
    }

    if (nameeparent) {
        // fat32_inode_put(ip);
        ip->i_op->iput(ip);
        return 0;
    }

    if (!ip->i_op) {
        // Log("ip iop invalid!");
        // ip->i_op = get_fat32_iops();
        ASSERT(cwd->fs_type==FAT32);
        ip->i_op = get_inodeops[cwd->fs_type]();
    }

    return ip;
}

struct inode* namei(char *path) {
    char name[NAME_LONG_MAX];
    return inode_namex(path, 0, name);
}

struct inode* namei_parent(char *path, char *name) {
    return inode_namex(path, 1, name);
}

static inline const struct inode_operations* get_fat32_iops(void) {
    static const struct inode_operations iops_instance = {
        .iunlock_put = fat32_inode_unlock_put,
        .iunlock = fat32_inode_unlock,
        .iput = fat32_inode_put,
        .ilock = fat32_inode_lock,
        .itrunc = fat32_inode_trunc,
        .iupdate = fat32_inode_update,
        .idirlookup = fat32_inode_dirlookup,
        .idelete = fat32_inode_delete,
        .idempty = fat32_isdirempty,
        .igetdents = fat32_getdents,
        .idup = fat32_inode_dup,
        .idir = fat32_isdir,
        .icreate = _fat32_inode_create,
    };

    return &iops_instance;
}

static inline const struct inode_operations* get_ext2_iops(void) {
    ASSERT(0);
    return NULL;
}

        // Not to be moved upward
const struct inode_operations* (*get_inodeops[])(void) = {
    [FAT32] get_fat32_iops,
    [EXT2] get_ext2_iops,
};