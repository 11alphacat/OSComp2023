#include "common.h"
#include "memory/writeback.h"
#include "proc/pdflush.h"
#include "lib/timer.h"

struct timer_list wb_timer;
    
static void background_writeout(uint64 _min_pages)
{
    uint64 nr_to_write = MAX_WRITEBACK_PAGES;

	for (;;) {
		writeback_inodes(nr_to_write);
		break;
	}
}

void wakeup_bdflush(void* nr_pages)
{
	pdflush_operation(background_writeout, (uint64)nr_pages);
}

// set timer to write back regularly
void page_writeback_timer_init(void) {
	wb_timer.count = -1;// not stop it 
	uint64 time_out = S_to_NS(dirty_writeback_cycle);
	add_timer_atomic(&wb_timer, time_out, wakeup_bdflush, 0);
}