#ifndef __SDCARD_H__
#define __SDCARD_H__
#include "fs/bio.h"


int sdcard_init();
void sdcard_rw(struct bio_vec *b, int write);


#endif