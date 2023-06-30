#ifndef __DISK_H__
#define __DISK_H__

struct buffer_head;
struct bio;

// #define BLOCK_SEL BLOCK_OLD
#define BLOCK_SEL BLOCK_NEW
#define BLOCK_OLD 0 // buffer header
#define BLOCK_NEW 1 // bio

// b is a pointer to struct bio_vec
void disk_rw(void *b, int write, int type);


#endif // __DISK_H__