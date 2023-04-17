#include "common.h"
#include "memory/list_alloc.h"
#include "atomic/spinlock.h"
#include "kernel/trap.h"
#include "debug.h"
#include "param.h"
#include "kernel/proc.h"
#include "kernel/pipe.h"
#include "fs/bio.h"
#include "fs/stat.h"
#include "fs/vfs/fs.h"
#include "fs/fat/fat32_disk.h"
#include "fs/fat/fat32_mem.h"
#include "fs/fat/fat32_stack.h"
#include "fs/fat/fat32_file.h"

#define F_WRITEABLE(fp)     ( ((fp)->f_mode & (O_WRONLY | O_RDWR)) == 0 ? 0 : 1)
#define F_READABLE(fp)     ( (fp)->f_mode == O_RDONLY || (fp)->f_mode == O_RDWR )

struct devsw devsw[NDEV];
struct {
  struct spinlock lock;
  struct _file file[NFILE];
} ftable;

void
fat32_fileinit(void)
{
  initlock(&ftable.lock, "ftable");
}

// Allocate a file structure.
// 语义：从内存中的 ftable 中寻找一个空闲的 file 项，并返回指向该 file 的指针
struct _file*
fat32_filealloc(void)
{
  struct _file *f;

  acquire(&ftable.lock);
  for(f = ftable.file; f < ftable.file + NFILE; f++){
    if(f->f_count == 0){
      f->f_count = 1;
      release(&ftable.lock);
      return f;
    }
  }
  release(&ftable.lock);
  return 0;
}

// Increment ref count for file f.
// 语义：将 f 指向的 file 文件的引用次数自增，并返回该指针
// 实现：给 ftable 加锁后，f->f_count++
struct _file*
fat32_filedup(struct _file *f)
{
  acquire(&ftable.lock);
  if(f->f_count < 1)
    panic("filedup");
  f->f_count++;
  release(&ftable.lock);
  return f;
}

// Close file f.  (Decrement ref count, close when reaches 0.)
// 语义：自减 f 指向的file的引用次数，如果为0，则关闭
// 对于管道文件，调用pipeclose
// 否则，调用iput归还inode节点
void
fat32_fileclose(struct _file *f)
{
  struct _file ff;

  acquire(&ftable.lock);
  if(f->f_count < 1)
    panic("fileclose");
  if(--f->f_count > 0){
    release(&ftable.lock);
    return;
  }
  ff = *f;
  f->f_count = 0;
  f->f_type = FD_NONE;
  release(&ftable.lock);

  if(ff.f_type == FD_PIPE){
    int wrable = F_WRITEABLE(&ff);
    pipeclose(ff.f_tp->f_pipe, wrable );
  } else if(ff.f_type == FD_INODE || ff.f_type == FD_DEVICE){
    // begin_op();
    fat32_inode_put(ff.f_tp->f_inode);
    // end_op();
  }
}

// Get metadata about file f.
// addr is a user virtual address, pointing to a struct stat.
// 语义：获取文件 f 的相关属性，写入 addr 指向的用户空间
int
fat32_filestat(struct _file *f, uint64 addr)
{
  struct proc *p = myproc();
  struct kstat st;
  
  if(f->f_type == FD_INODE || f->f_type == FD_DEVICE){
    fat32_inode_lock(f->f_tp->f_inode);
    fat32_inode_stati(f->f_tp->f_inode, &st);
    fat32_inode_unlock(f->f_tp->f_inode);
    if(copyout(p->pagetable, addr, (char *)&st, sizeof(st)) < 0)
      return -1;
    return 0;
  }
  return -1;
}

// Read from file f.
// addr is a user virtual address.
// 语义：读取文件 f ，从 偏移量 f->f_pos 起始，读取 n 个字节到 addr 指向的用户空间 
int
fat32_fileread(struct _file *f, uint64 addr, int n)
{
  int r = 0;

  if(F_READABLE(f) == 0)
    return -1;

  if(f->f_type == FD_PIPE){
    r = piperead(f->f_tp->f_pipe, addr, n);
  } else if(f->f_type == FD_DEVICE){
    if(f->f_major < 0 || f->f_major >= NDEV || !devsw[f->f_major].read)
      return -1;
    r = devsw[f->f_major].read(1, addr, n);
  } else if(f->f_type == FD_INODE){
    fat32_inode_lock(f->f_tp->f_inode);
    if((r = fat32_inode_read(f->f_tp->f_inode, 1, addr, f->f_pos, n)) > 0)
      f->f_pos += r;
    fat32_inode_unlock(f->f_tp->f_inode);
  } else {
    panic("fileread");
  }

  return r;
}

// Write to file f.
// addr is a user virtual address.
// 语义：写文件 f ，从 f->f_pos开始，把用户空间 addr 起始的 n 个字节的内容写入文件 f
int
fat32_filewrite(struct _file *f, uint64 addr, int n)
{
  int r, ret = 0;

  if(F_WRITEABLE(f) == 0)
    return -1;

  if(f->f_type == FD_PIPE){
    ret = pipewrite(f->f_tp->f_pipe, addr, n);
  } else if(f->f_type == FD_DEVICE){
    if(f->f_major < 0 || f->f_major >= NDEV || !devsw[f->f_major].write)
      return -1;
    ret = devsw[f->f_major].write(1, addr, n);
  } else if(f->f_type == FD_INODE){
    // write a few blocks at a time to avoid exceeding
    // the maximum log transaction size, including
    // i-node, indirect block, allocation blocks,
    // and 2 blocks of slop for non-aligned writes.
    // this really belongs lower down, since writei()
    // might be writing a device like the console.
    int max = ((MAXOPBLOCKS-1-1-2) / 2) * BSIZE;
    int i = 0;
    while(i < n){
      int n1 = n - i;
      if(n1 > max)
        n1 = max;

      // begin_op();
      ilock(f->f_tp->f_inode);
      if ((r = writei(f->f_tp->f_inode, 1, addr + i, f->f_pos, n1)) > 0)
        f->f_pos += r;
      iunlock(f->f_tp->f_inode);
      // end_op();

      if(r != n1){
        // error from writei
        break;
      }
      i += r;
    }
    ret = (i == n ? n : -1);
  } else {
    panic("filewrite");
  }

  return ret;
}