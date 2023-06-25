#include "common.h"
#include "param.h"
#include "atomic/spinlock.h"
#include "lib/riscv.h"
#include "fs/bio.h"
#include "driver/virtio.h"
#include "lib/list.h"
#include "memory/allocator.h"

struct {
    struct spinlock lock;
    struct buffer_head buf[NBUF];
    list_head_t head;
} bcache;

void binit(void) {
    struct buffer_head *b;

    initlock(&bcache.lock, "bcache");

    INIT_LIST_HEAD(&bcache.head);
    for (b = bcache.buf; b < bcache.buf + NBUF; b++) {
        sema_init(&b->sem_lock, 1, "buffer");
        sema_init(&b->sem_disk_done, 0, "buffer_disk_done");
        list_add(&b->lru, &bcache.head);
    }
}

// Look through buffer cache for block on device dev.
// If not found, allocate a buffer.
// In either case, return locked buffer.
static struct buffer_head *bget(uint dev, uint blockno) {
    struct buffer_head *b;

    acquire(&bcache.lock);

    // Is the block already cached?
    list_for_each_entry(b, &bcache.head, lru) {
        if (b->dev == dev && b->blockno == blockno) {
            atomic_inc_return(&b->refcnt);
            release(&bcache.lock);
            sema_wait(&b->sem_lock);
            return b;
        }
    }

    // Not cached.
    // Recycle the least recently used (LRU) unused buffer.
    list_for_each_entry_reverse(b, &bcache.head, lru) {
        if (atomic_read(&b->refcnt) == 0) {
            b->dev = dev;
            b->blockno = blockno;
            b->valid = 0;
            atomic_set(&b->refcnt, 1);
            release(&bcache.lock);
            sema_wait(&b->sem_lock);
            return b;
        }
    }
    panic("bget: no buffers");
}

// Return a locked buf with the contents of the indicated block.
struct buffer_head *bread(uint dev, uint blockno) {
    struct buffer_head *b;

    b = bget(dev, blockno);

    if (!b->valid) {
        disk_rw_bio(b, DISK_READ);
        b->valid = 1;
        b->dirty = 0;
    }
    return b;
}

// Write b's contents to disk.  Must be locked.
void bwrite(struct buffer_head *b) {
    b->dirty = 1;
}

// Release a locked buffer.
// Move to the head of the most-recently-used list.
void brelse(struct buffer_head *b) {
    if (b->dirty == 1) {
        disk_rw_bio(b, DISK_WRITE);
        b->dirty = 0;
    }
    sema_signal(&b->sem_lock);

    acquire(&bcache.lock);
    atomic_dec_return(&b->refcnt);
    if (atomic_read(&b->refcnt) == 0) {
        list_del(&b->lru);
        list_add(&b->lru, &bcache.head);
    }

    release(&bcache.lock);
}

// rw : DISK_READ or DISK_WRITE
void disk_rw_bio(struct buffer_head *b, int rw) {
    struct bio bio_new;
    struct bio_vec vec_new;
    init_bio(&bio_new, &vec_new, b, rw);
    submit_bio(&bio_new);
}

void init_bio(struct bio *bio_p, struct bio_vec *vec_p, struct buffer_head *b, int rw) {
    // bio_vec
    sema_init(&vec_p->sem_disk_done, 0, "bio_disk_done"); // vec_p ！！！
    vec_p->blockno_start = b->blockno;
    vec_p->block_len = 1;
    vec_p->data = b->data;
    vec_p->disk = b->disk;
    // bio
    bio_p->bi_io_vec = vec_p;
    bio_p->bi_idx = 0;
    bio_p->bi_vcnt = 1;
    bio_p->bi_rw = rw;
    bio_p->bi_bdev = b->dev;
}

// when using kalloc, don't forget free!!!
void free_bio(struct bio *bio) {
    struct bio_vec *vec;
    for (vec = bio->bi_io_vec; vec < &bio->bi_io_vec[bio->bi_vcnt]; vec++) {
        kfree(vec);
    }
    kfree(bio);
}

// read or write
void submit_bio(struct bio *bio) {
    struct bio_vec *vec;
    for (vec = bio->bi_io_vec; vec < &bio->bi_io_vec[bio->bi_vcnt]; vec++) {
        virtio_disk_rw((void *)vec, bio->bi_rw, BLOCK_SEL);
    }
}