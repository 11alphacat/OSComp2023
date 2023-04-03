// Buffer cache.
//
// The buffer cache is a linked list of buf structures holding
// cached copies of disk block contents.  Caching disk blocks
// in memory reduces the number of disk reads and also provides
// a synchronization point for disk blocks used by multiple processes.
//
// Interface:
// * To get a buffer for a particular disk block, call bread.
// * After changing buffer data, call bwrite to write it to disk.
// * When done with the buffer, call brelse.
// * Do not use the buffer after calling brelse.
// * Only one process at a time can use a buffer,
//     so do not keep them longer than necessary.

#include "common.h"
#include "param.h"
#include "atomic/spinlock.h"
#include "atomic/sleeplock.h"
#include "riscv.h"
#include "fs/inode/fs.h"
#include "fs/bio.h"
#include "driver/virtio.h"
#include "list.h"

struct {
    struct spinlock lock;
    struct buffer_head buf[NBUF];

    list_head_t head;
} bcache;

#define DISK_WRITE 1 // write disk
#define DISK_READ 0  // read disk
#define BLOCK_OLD 0  // buffer header
#define BLOCK_NEW 1  // bio

void binit(void) {
    struct buffer_head *b;

    initlock(&bcache.lock, "bcache");

    INIT_LIST_HEAD(&bcache.head);
    for (b = bcache.buf; b < bcache.buf + NBUF; b++) {
        initsleeplock(&b->lock, "buffer");
        list_add(&b->lru, &bcache.head);
    }
}

// Look through buffer cache for block on device dev.
// If not found, allocate a buffer.
// In either case, return locked buffer.
static struct buffer_head *
bget(uint dev, uint blockno) {
    struct buffer_head *b;

    acquire(&bcache.lock);

    // Is the block already cached?
    list_for_each_entry(b, &bcache.head, lru) {
        if (b->dev == dev && b->blockno == blockno) {
            atomic_inc_return(&b->refcnt);
            release(&bcache.lock);
            acquiresleep(&b->lock);
            return b;
        }
    }

    // Not cached.
    // Recycle the least recently used (LRU) unused buffer.
    // for (b = bcache.head.prev; b != &bcache.head; b = b->prev) {
    list_for_each_entry_reverse(b, &bcache.head, lru) {
        if (atomic_read(&b->refcnt) == 0) {
            b->dev = dev;
            b->blockno = blockno;
            b->valid = 0;
            atomic_set(&b->refcnt, 1);
            release(&bcache.lock);
            acquiresleep(&b->lock);
            return b;
        }
    }
    panic("bget: no buffers");
}

// Return a locked buf with the contents of the indicated block.
struct buffer_head *
bread(uint dev, uint blockno) {
    struct buffer_head *b;

    b = bget(dev, blockno);
    if (!b->valid) {
        virtio_disk_rw(b, DISK_READ);
        b->valid = 1;
        b->dirty = 0;
    }
    return b;
}

// Write b's contents to disk.  Must be locked.
void bwrite(struct buffer_head *b) {
    if (!holdingsleep(&b->lock))
        panic("bwrite");
    // virtio_disk_rw(b, 1);
    b->dirty = 1;
}

// Release a locked buffer.
// Move to the head of the most-recently-used list.
void brelse(struct buffer_head *b) {
    if (!holdingsleep(&b->lock))
        panic("brelse");
    if (b->dirty == 1) {
        virtio_disk_rw(b, DISK_WRITE);
        b->dirty = 0;
    }
    releasesleep(&b->lock);

    acquire(&bcache.lock);
    atomic_dec_return(&b->refcnt);
    if (atomic_read(&b->refcnt) == 0) {
        list_del(&b->lru);
        list_add(&b->lru, &bcache.head);
    }

    release(&bcache.lock);
}

int bpin(struct buffer_head *b) {
    int inc_ret = atomic_inc_return(&b->refcnt);
    return inc_ret;
}

int bunpin(struct buffer_head *b) {
    int dec_ret = atomic_dec_return(&b->refcnt);
    return dec_ret;
}

int submit_bio(int rw, struct bio *bio) {
    bio->bi_rw |= rw;
    return 0;
}

int init_bio() {
    return 0;
}

int free_bio(struct bio *bio) {
    return 0;
}