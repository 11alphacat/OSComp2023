#ifndef __WAIT_QUEUE_H__
#define __WAIT_QUEUE_H__



void sleep(void *, struct spinlock *);
void wakeup(void *);


#endif