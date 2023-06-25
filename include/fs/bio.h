#ifndef __BIO_H__
#define __BIO_H__

#include "common.h"
#include "lib/list.h"
#include "atomic/ops.h"
#include "atomic/semaphore.h"
#include "fs/vfs/fs_macro.h"
#include "lib/list.h"

#define DISK_WRITE 1 // write disk
#define DISK_READ 0  // read disk

struct buffer_head {
    struct semaphore sem_lock;
    struct semaphore sem_disk_done;
    uint blockno;
    atomic_t refcnt;
    list_head_t lru; // LRU cache list
    uchar data[BSIZE];
    int valid; // has data been read from disk?
    int dirty; // dirty
    int disk;  // does disk "own" buf?
    uint dev;
};

struct bio {
    uint bi_bdev;              // device no
    uint64 bi_rw;              // read or write
    ushort bi_idx;             // current idx
    ushort bi_vcnt;            // total number
    struct bio_vec *bi_io_vec; // bio vecs list
};

// different from Linux
struct bio_vec {
    struct semaphore sem_disk_done;
    uint blockno_start;
    uint block_len;
    uchar *data;
    int disk;
};
// no_start : 2
// len : 4
// -> 2 3 4 5 (a series of blocks)

// xv6 origin :  buffer_head
void binit(void);
struct buffer_head *bread(uint, uint);
void brelse(struct buffer_head *);
void bwrite(struct buffer_head *);
// bio
void disk_rw_bio(struct buffer_head *b, int rw);
void init_bio(struct bio *bio_p, struct bio_vec *vec_p, struct buffer_head *b, int rw);
void submit_bio(struct bio *bio);
void free_bio(struct bio *bio);

#endif // __BIO_H__