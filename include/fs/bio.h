#ifndef __BIO_H__
#define __BIO_H__

#include "common.h"
#include "list.h"
#include "atomic/atomic.h"
#include "atomic/sleeplock.h"
#include "fs/inode/fs_macro.h"

enum bh_state_bits {
    BH_Uptodate, /* Contains valid data */
    BH_Dirty,    /* Is dirty */
    BH_Lock,     /* Is locked */
    BH_Req,      /* Has been submitted for I/O */
};

struct buffer_head {
    struct sleeplock lock;
    uint64 b_state; // buffer state
    int valid;      // has data been read from disk?
    int disk;       // does disk "own" buf?
    uint dev;
    uint blockno;
    atomic_t refcnt;
    // struct buffer_head *prev; // LRU cache list
    // struct buffer_head *next;
    list_head_t lru;
    uchar data[BSIZE];
    int dirty; // dirty
};

struct bio_vec {
    // <page,offset,len>
    struct page *bv_page;
    uint bv_len;
    uint bv_offset;
};

struct bio {
    atomic_t bi_cnt;           // ref
    ushort bi_idx;             // current idx
    struct bio_vec *bi_io_vec; // bio vecs list
    uint64 bi_rw;              // read or write
    uint bi_bdev;              // device no
    ushort bi_vcnt;            // total number
};

// bio.c
void binit(void);
struct buffer_head *bread(uint, uint);
void brelse(struct buffer_head *);
void bwrite(struct buffer_head *);
int bpin(struct buffer_head *);
int bunpin(struct buffer_head *);

int init_bio(void);
int submit_bio(int, struct bio *);
int free_bio(struct bio *);

#endif // __BIO_H__