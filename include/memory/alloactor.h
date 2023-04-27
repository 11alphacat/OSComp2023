#ifndef __ALLOACTOR_H__
#define __ALLOACTOR_H__

/* reserve this to be compatible with the old kalloc call
 * use kmalloc(PGSIZE) instead 
 */
void *kalloc(void);

void kfree(void *);
void *kzalloc(size_t size);
void *kmalloc(size_t size);

/* get available memory size */
uint64 get_free_mem();

#endif // __ALLOACTOR_H__