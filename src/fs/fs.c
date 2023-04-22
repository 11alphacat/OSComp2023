// File system implementation.  Five layers:
//   + Blocks: allocator for raw disk blocks.
//   + Log: crash recovery for multi-step updates.
//   + Files: inode allocator, reading, writing, metadata.
//   + Directories: inode with special contents (list of other inodes!)
//   + Names: paths like /usr/rtm/xv6/fs.c for convenient naming.
//
// This file contains the low-level file system manipulation
// routines.  The (higher-level) system call implementations
// are in sysfile.c.

#include "common.h"
#include "riscv.h"
#include "param.h"
#include "atomic/spinlock.h"
#include "kernel/proc.h"
#include "atomic/sleeplock.h"
#include "fs/bio.h"
#include "fs/stat.h"
#include "fs/inode/fs.h"
#include "fs/inode/file.h"
#include "fs/inode/stat.h"

#define min(a, b) ((a) < (b) ? (a) : (b))
// there should be one superblock per disk device, but we run with
// only one device
struct superblock sb;

// Read the super block.
static void
readsb(int dev, struct superblock *sb) {
    struct buffer_head *bp;

    bp = bread(dev, 1);
    memmove(sb, bp->data, sizeof(*sb));
    brelse(bp);
}

// Init fs
void fsinit(int dev) {
    readsb(dev, &sb);
    if (sb.magic != FSMAGIC)
        panic("invalid file system");
    initlog(dev, &sb);
}

// Zero a block.
static void
bzero(int dev, int bno) {
    struct buffer_head *bp;

    bp = bread(dev, bno);
    memset(bp->data, 0, BSIZE);
    log_write(bp);
    brelse(bp);
}

// Blocks.

// Allocate a zeroed disk block.
// returns 0 if out of disk space.
static uint
balloc(uint dev) {
    int b, bi, m;
    struct buffer_head *bp;

    bp = 0;
    for (b = 0; b < sb.size; b += BPB) { // 这个循环只走一次，this loop goes only once
        bp = bread(dev, BBLOCK(b, sb));  // 这里是从磁盘的bitmap读了一个block到内存的bcache中

        for (bi = 0; bi < BPB && b + bi < sb.size; bi++) {
            m = 1 << (bi % 8);
            if ((bp->data[bi / 8] & m) == 0) { // Is block free?
                bp->data[bi / 8] |= m;         // Mark block in use.
                log_write(bp);
                brelse(bp);
                // bzero(dev, b + bi);    // origin
                bzero(dev, b % BPB + bi); // change
                return b + bi;
            }
        }
        brelse(bp);
    }
    printf("balloc: out of blocks\n");
    return 0;
}

// Free a disk block.
static void
bfree(int dev, uint b) {
    struct buffer_head *bp;
    int bi, m;

    bp = bread(dev, BBLOCK(b, sb));
    bi = b % BPB;
    m = 1 << (bi % 8);
    if ((bp->data[bi / 8] & m) == 0)
        panic("freeing free block");
    bp->data[bi / 8] &= ~m;
    log_write(bp);
    brelse(bp);
}

// Inodes.
//
// An inode describes a single unnamed file.
// The inode disk structure holds metadata: the file's type,
// its size, the number of links referring to it, and the
// list of blocks holding the file's content.
//
// The inodes are laid out sequentially on disk at block
// sb.inodestart. Each inode has a number, indicating its
// position on the disk.
//
// The kernel keeps a table of in-use inodes in memory
// to provide a place for synchronizing access
// to inodes used by multiple processes. The in-memory
// inodes include book-keeping information that is
// not stored on disk: ip->ref and ip->valid.
//
// An inode and its in-memory representation go through a
// sequence of states before they can be used by the
// rest of the file system code.
//
// * Allocation: an inode is allocated if its type (on disk)
//   is non-zero. ialloc() allocates, and iput() frees if
//   the reference and link counts have fallen to zero.
//
// * Referencing in table: an entry in the inode table
//   is free if ip->ref is zero. Otherwise ip->ref tracks
//   the number of in-memory pointers to the entry (open
//   files and current directories). iget() finds or
//   creates a table entry and increments its ref; iput()
//   decrements ref.
//
// * Valid: the information (type, size, &c) in an inode
//   table entry is only correct when ip->valid is 1.
//   ilock() reads the inode from
//   the disk and sets ip->valid, while iput() clears
//   ip->valid if ip->ref has fallen to zero.
//
// * Locked: file system code may only examine and modify
//   the information in an inode and its content if it
//   has first locked the inode.
//
// Thus a typical sequence is:
//   ip = iget(dev, inum)
//   ilock(ip)
//   ... examine and modify ip->xxx ...
//   iunlock(ip)
//   iput(ip)
//
// ilock() is separate from iget() so that system calls can
// get a long-term reference to an inode (as for an open file)
// and only lock it for short periods (e.g., in read()).
// The separation also helps avoid deadlock and races during
// pathname lookup. iget() increments ip->ref so that the inode
// stays in the table and pointers to it remain valid.
//
// Many internal file system functions expect the caller to
// have locked the inodes involved; this lets callers create
// multi-step atomic operations.
//
// The itable.lock spin-lock protects the allocation of itable
// entries. Since ip->ref indicates whether an entry is free,
// and ip->dev and ip->inum indicate which i-node an entry
// holds, one must hold itable.lock while using any of those fields.
//
// An ip->lock sleep-lock protects all ip-> fields other than ref,
// dev, and inum.  One must hold ip->lock in order to
// read or write that inode's ip->valid, ip->size, ip->type, &c.

struct {
    struct spinlock lock;
    struct inode inode[NINODE];
} itable;

void iinit() {
    int i = 0;

    initlock(&itable.lock, "itable");
    for (i = 0; i < NINODE; i++) {
        initsleeplock(&itable.inode[i].lock, "inode");
    }
}

static struct inode *iget(uint dev, uint inum);

// Allocate an inode on device dev.
// Mark it as allocated by  giving it type type.
// Returns an unlocked but allocated and referenced inode,
// or NULL if there is no free inode.
// 语义：获取一个空闲的 inode 节点来持久化一个文件
struct inode *
ialloc(uint dev, short type) {
    int inum;
    struct buffer_head *bp;
    struct dinode *dip;

    for (inum = 1; inum < sb.ninodes; inum++) {
        bp = bread(dev, IBLOCK(inum, sb));
        dip = (struct dinode *)bp->data + inum % IPB;
        if (dip->type == 0) { // a free inode
            memset(dip, 0, sizeof(*dip));
            dip->type = type;
            log_write(bp); // mark it allocated on the disk
            brelse(bp);
            return iget(dev, inum);
        }
        brelse(bp);
    }

    printf("ialloc: no inodes\n");
    return 0;
}

// Copy a modified in-memory inode to disk.
// Must be called after every change to an ip->xxx field
// that lives on disk.
// Caller must hold ip->lock.
void iupdate(struct inode *ip) {
    struct buffer_head *bp;
    struct dinode *dip;

    bp = bread(ip->dev, IBLOCK(ip->inum, sb));
    dip = (struct dinode *)bp->data + ip->inum % IPB;
    dip->type = ip->type;
    dip->major = ip->major;
    dip->minor = ip->minor;
    dip->nlink = ip->nlink;
    dip->size = ip->size;
    memmove(dip->addrs, ip->addrs, sizeof(ip->addrs));
    log_write(bp);
    brelse(bp);
}

// Find the inode with number inum on device dev
// and return the in-memory copy. Does not lock
// the inode and does not read it from disk.
// 语义：通过 dev 和 inum 拿到 inode 指针
static struct inode *
iget(uint dev, uint inum) {
    struct inode *ip, *empty;

    acquire(&itable.lock);

    // Is the inode already in the table?
    empty = 0;
    for (ip = &itable.inode[0]; ip < &itable.inode[NINODE]; ip++) { // 虽然inum在磁盘中很容易定位，但内存中缓存的inode不一定是磁盘中的序列
        if (ip->ref > 0 && ip->dev == dev && ip->inum == inum) {    // 所以需要循环一个一个比较
            ip->ref++;
            release(&itable.lock);
            return ip;
        }
        if (empty == 0 && ip->ref == 0) // Remember empty slot.
            empty = ip;
    }

    // Recycle an inode entry.
    if (empty == 0)
        panic("iget: no inodes");

    ip = empty;
    ip->dev = dev;
    ip->inum = inum;
    ip->ref = 1;
    ip->valid = 0;
    release(&itable.lock);

    return ip;
}

// Increment reference count for ip.
// Returns ip to enable ip = idup(ip1) idiom.
struct inode *
idup(struct inode *ip) {
    acquire(&itable.lock);
    ip->ref++;
    release(&itable.lock);
    return ip;
}

// Lock the given inode.
// Reads the inode from disk if necessary.
void ilock(struct inode *ip) {
    struct buffer_head *bp;
    struct dinode *dip;

    if (ip == 0 || ip->ref < 1)
        panic("ilock");

    acquiresleep(&ip->lock);

    if (ip->valid == 0) {
        bp = bread(ip->dev, IBLOCK(ip->inum, sb));
        dip = (struct dinode *)bp->data + ip->inum % IPB;
        ip->type = dip->type;
        ip->major = dip->major;
        ip->minor = dip->minor;
        ip->nlink = dip->nlink;
        ip->size = dip->size;
        memmove(ip->addrs, dip->addrs, sizeof(ip->addrs));
        brelse(bp);
        ip->valid = 1;
        if (ip->type == 0)
            panic("ilock: no type");
    }
}

// Unlock the given inode.
void iunlock(struct inode *ip) {
    if (ip == 0 || !holdingsleep(&ip->lock) || ip->ref < 1)
        panic("iunlock");

    releasesleep(&ip->lock);
}

// Drop a reference to an in-memory inode.
// If that was the last reference, the inode table entry can
// be recycled.
// If that was the last reference and the inode has no links
// to it, free the inode (and its content) on disk.
// All calls to iput() must be inside a transaction in
// case it has to free the inode.
void iput(struct inode *ip) {
    acquire(&itable.lock);

    if (ip->ref == 1 && ip->valid && ip->nlink == 0) {
        // inode has no links and no other references: truncate and free.

        // ip->ref == 1 means no other process can have ip locked,
        // so this acquiresleep() won't block (or deadlock).
        acquiresleep(&ip->lock);

        release(&itable.lock);

        itrunc(ip);
        ip->type = 0;
        iupdate(ip);
        ip->valid = 0;

        releasesleep(&ip->lock);

        acquire(&itable.lock);
    }

    ip->ref--;
    release(&itable.lock);
}

// Common idiom: unlock, then put.
void iunlockput(struct inode *ip) {
    iunlock(ip);
    iput(ip);
}

// Inode content
//
// The content (data) associated with each inode is stored
// in blocks on the disk. The first NDIRECT block numbers
// are listed in ip->addrs[].  The next NINDIRECT blocks are
// listed in block ip->addrs[NDIRECT].

// Return the disk block address of the nth block in inode ip.
// If there is no such block, bmap allocates one.
// returns 0 if out of disk space.
static uint
bmap(struct inode *ip, uint bn) {
    uint addr, *a;
    struct buffer_head *bp;

    if (bn < NDIRECT) {
        if ((addr = ip->addrs[bn]) == 0) {
            addr = balloc(ip->dev);
            if (addr == 0)
                return 0;
            ip->addrs[bn] = addr;
        }
        return addr;
    }
    bn -= NDIRECT;
    // 如果我们想要将文件索引改为混合索引（直接+3级索引），只需要修改bmap函数
    // if we want to convert the index method used in this function to another mixed index method,
    // i.e., direct index plusing 3-level index, we just modify this bmap function
    if (bn < NINDIRECT) {
        // Load indirect block, allocating if necessary.
        if ((addr = ip->addrs[NDIRECT]) == 0) {
            addr = balloc(ip->dev);
            if (addr == 0)
                return 0;
            ip->addrs[NDIRECT] = addr;
        }
        bp = bread(ip->dev, addr);
        a = (uint *)bp->data;
        if ((addr = a[bn]) == 0) {
            addr = balloc(ip->dev);
            if (addr) {
                a[bn] = addr;
                log_write(bp);
            }
        }
        brelse(bp);
        return addr;
    }

    panic("bmap: out of range");
}

// Truncate inode (discard contents).
// Caller must hold ip->lock.
void itrunc(struct inode *ip) {
    int i, j;
    struct buffer_head *bp;
    uint *a;

    for (i = 0; i < NDIRECT; i++) {
        if (ip->addrs[i]) {
            bfree(ip->dev, ip->addrs[i]);
            ip->addrs[i] = 0;
        }
    }

    if (ip->addrs[NDIRECT]) {
        bp = bread(ip->dev, ip->addrs[NDIRECT]);
        a = (uint *)bp->data;
        for (j = 0; j < NINDIRECT; j++) {
            if (a[j])
                bfree(ip->dev, a[j]);
        }
        brelse(bp);
        bfree(ip->dev, ip->addrs[NDIRECT]);
        ip->addrs[NDIRECT] = 0;
    }

    ip->size = 0;
    iupdate(ip);
}

// Copy stat information from inode.
// Caller must hold ip->lock.
void stati(struct inode *ip, struct stat *st) {
    st->dev = ip->dev;
    st->ino = ip->inum;
    st->type = ip->type;
    st->nlink = ip->nlink;
    st->size = ip->size;
}

// Read data from inode.
// Caller must hold ip->lock.
// If user_dst==1, then dst is a user virtual address;
// otherwise, dst is a kernel address.
// 语义：想要读取 ip 指向的文件，从偏移量 off 起始，读取 n 个字节到 dst 中
int readi(struct inode *ip, int user_dst, uint64 dst, uint off, uint n) {
    uint tot, m;
    struct buffer_head *bp;

    if (off > ip->size || off + n < off)
        return 0;
    if (off + n > ip->size)
        n = ip->size - off;

    for (tot = 0; tot < n; tot += m, off += m, dst += m) { // dst += m
        uint addr = bmap(ip, off / BSIZE);                 // 查询该文件第 off/BSIZE 个磁盘块的地址
        if (addr == 0)
            break;
        bp = bread(ip->dev, addr);             // 读取该地址起始的一个块大小，返回一个buf结构
        m = min(n - tot, BSIZE - off % BSIZE); // 计算这次实际要读的长度
        if (either_copyout(user_dst, dst, bp->data + (off % BSIZE), m) == -1) {
            brelse(bp);
            tot = -1;
            break;
        }
        brelse(bp);
    }
    return tot;
}

// Write data to inode.
// Caller must hold ip->lock.
// If user_src==1, then src is a user virtual address;
// otherwise, src is a kernel address.
// Returns the number of bytes successfully written.
// If the return value is less than the requested n,
// there was an error of some kind.
// 语义：想要写 ip 指向的文件，从偏移量 off 起始，从 src 中写 n 个字节到文件里
int writei(struct inode *ip, int user_src, uint64 src, uint off, uint n) {
    uint tot, m;
    struct buffer_head *bp;

    if (off > ip->size || off + n < off)
        return -1;
    if (off + n > MAXFILE * BSIZE)
        return -1;

    for (tot = 0; tot < n; tot += m, off += m, src += m) { // src += m
        uint addr = bmap(ip, off / BSIZE);                 // 查询该文件第 off/BSIZE 个磁盘块的地址
        if (addr == 0)
            break;
        bp = bread(ip->dev, addr);             // 读取该地址起始的一个块大小，返回一个buf结构
        m = min(n - tot, BSIZE - off % BSIZE); // 计算这次实际要写的长度
        if (either_copyin(bp->data + (off % BSIZE), user_src, src, m) == -1) {
            brelse(bp);
            break;
        }
        log_write(bp);
        brelse(bp);
    }

    if (off > ip->size)
        ip->size = off;

    // write the i-node back to disk even if the size didn't change
    // because the loop above might have called bmap() and added a new
    // block to ip->addrs[].
    iupdate(ip);

    return tot;
}

// Directories

int namecmp(const char *s, const char *t) {
    return strncmp(s, t, DIRSIZ);
}

// Look for a directory entry in a directory.
// If found, set *poff to byte offset of entry.
// 语义：在 dp 目录文件的data区内找一个 目录项
// 实现：通过readi读取 dp 指向的文件data，每次读 sizeof(dirent)，判断是否名字匹配
// 如果匹配，则记录该dirent结构中的 inum， 使用 iget 函数找到指向节点值为 inum 的 ip 指针
// 即我们想要找的目录文件
struct inode *
dirlookup(struct inode *dp, char *name, uint *poff) {
    uint off, inum;
    struct dirent de;

    if (dp->type != T_DIR)
        panic("dirlookup not DIR");

    for (off = 0; off < dp->size; off += sizeof(de)) {
        if (readi(dp, 0, (uint64)&de, off, sizeof(de)) != sizeof(de))
            panic("dirlookup read");
        if (de.inum == 0) // free
            continue;
        if (namecmp(name, de.name) == 0) {
            // entry matches path element
            if (poff)
                *poff = off;
            inum = de.inum;
            return iget(dp->dev, inum);
        }
    }

    return 0;
}

// Write a new directory entry (name, inum) into the directory dp.
// Returns 0 on success, -1 on failure (e.g. out of disk blocks).
// 语义：在dp所指的目录文件的data区中，写入一个内容为 {inum, name} 的 dirent 结构
// 实现：首先调用 dirlookup 函数在dp数据区中查询，是否已经存在名字为 name 的目录
// 如果不存在，则循环调用 readi 函数，在 dp 的数据区中找一个空闲的位置（获取off）
// 如果找到了，就调用 writei 函数往 dp 的偏移量为 off 的位置，写入一个内容为 {inum, name} 的 dirent 项
int dirlink(struct inode *dp, char *name, uint inum) {
    int off;
    struct dirent de;
    struct inode *ip;

    // Check that name is not present.
    if ((ip = dirlookup(dp, name, 0)) != 0) {
        iput(ip);
        return -1;
    }

    // Look for an empty dirent.
    for (off = 0; off < dp->size; off += sizeof(de)) {
        if (readi(dp, 0, (uint64)&de, off, sizeof(de)) != sizeof(de))
            panic("dirlink read");
        if (de.inum == 0)
            break;
    }

    strncpy(de.name, name, DIRSIZ);
    de.inum = inum;
    if (writei(dp, 0, (uint64)&de, off, sizeof(de)) != sizeof(de))
        return -1;

    return 0;
}

// Paths

// Copy the next path element from path into name.
// Return a pointer to the element following the copied one.
// The returned path has no leading slashes,
// so the caller can check *path=='\0' to see if the name is the last one.
// If no name to remove, return 0.
//
// Examples:
//   skipelem("a/bb/c", name) = "bb/c", setting name = "a"
//   skipelem("///a//bb", name) = "bb", setting name = "a"
//   skipelem("a", name) = "", setting name = "a"
//   skipelem("", name) = skipelem("////", name) = 0
//
static char *
skipelem(char *path, char *name) {
    char *s;
    int len;

    while (*path == '/')
        path++;
    if (*path == 0)
        return 0;
    s = path;
    while (*path != '/' && *path != 0)
        path++;
    len = path - s;
    if (len >= DIRSIZ)
        memmove(name, s, DIRSIZ);
    else {
        memmove(name, s, len);
        name[len] = 0;
    }
    while (*path == '/')
        path++;
    return path;
}

// Look up and return the inode for a path name.
// If parent != 0, return the inode for the parent and copy the final
// path element into name, which must have room for DIRSIZ bytes.
// Must be called inside a transaction since it calls iput().
// 语义：传入一个 path，该函数能够找到 path 代表的文件，返回指向该文件 inode 的 ip 指针
// 并且将文件名保存在 name 中
static struct inode *
namex(char *path, int nameiparent, char *name) {
    struct inode *ip, *next;

    if (*path == '/')
        ip = iget(ROOTDEV, ROOTINO);
    else
        ip = idup(myproc()->cwd);

    while ((path = skipelem(path, name)) != 0) {
        ilock(ip);
        if (ip->type != T_DIR) {
            iunlockput(ip);
            return 0;
        }
        if (nameiparent && *path == '\0') {
            // Stop one level early.
            iunlock(ip);
            return ip;
        }
        if ((next = dirlookup(ip, name, 0)) == 0) {
            iunlockput(ip);
            return 0;
        }
        iunlockput(ip);
        ip = next;
    }
    if (nameiparent) {
        iput(ip);
        return 0;
    }
    return ip;
}

// 语义：传入一个 path，该函数能够找到 path 代表的文件，返回指向该文件 inode 的 ip 指针
// namei 返回路径名中最后一个元素的inode；
struct inode *
namei(char *path) {
    char name[DIRSIZ];
    return namex(path, 0, name);
}

// 语义：nameiparent 返回最后一个元素的父目录的inode
// 并且将最后一个元素的名称复制到调用者指定的位置 *name 中
struct inode *
nameiparent(char *path, char *name) {
    return namex(path, 1, name);
}
