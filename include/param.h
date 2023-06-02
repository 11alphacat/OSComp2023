#ifndef __PARAM_H__
#define __PARAM_H__

#define NPROC 32 // maximum number of processes
#define NTCB_PER_PROC 10
#define NTCB ((NPROC) * (NTCB_PER_PROC))
#define NCPU 4     // maximum number of CPUs
#define NOFILE 128 // open files per process
#define NFILE 100  // open files per system

#define NINODE 100                // maximum number of active i-nodes
#define NDEV 10                   // maximum major device number
#define ROOTDEV 1                 // device number of file system root disk
#define MAXARG 32                 // max exec arguments
#define MAXENV 32                 // max exec environment arguments
#define MAXOPBLOCKS 10            // max # of blocks any FS op writes
#define LOGSIZE (MAXOPBLOCKS * 3) // max data blocks in on-disk log
#define NBUF (MAXOPBLOCKS * 3)    // size of disk block cache
#define FSSIZE 2000               // size of file system in blocks
#define MAXPATH 128               // maximum file path name

#define NAME_LONG_MAX 255
#define PATH_LONG_MAX 260
#define FUTEX_NUM 32 // number of futex queue

// // in xv6
// #define MAXPATH 128 // maximum file path name
#endif // __PARAM_H__
