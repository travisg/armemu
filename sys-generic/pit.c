/*
 * Copyright (c) 2005 Travis Geiselbrecht
 *
 * Permission is hereby granted, free of charge, to any person obtaining
 * a copy of this software and associated documentation files
 * (the "Software"), to deal in the Software without restriction,
 * including without limitation the rights to use, copy, modify, merge,
 * publish, distribute, sublicense, and/or sell copies of the Software,
 * and to permit persons to whom the Software is furnished to do so,
 * subject to the following conditions:
 * 
 * The above copyright notice and this permission notice shall be
 * included in all copies or substantial portions of the Software.
 * 
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
 * IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY
 * CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,
 * TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE
 * SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 */
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <sys/types.h>

#include <SDL/SDL.h>
#include <SDL/SDL_thread.h>

#include <arm/arm.h>
#include <sys/sys.h>
#include <util/endian.h>
#include "sys_p.h"

static struct pit {
	SDL_mutex *mutex;
	SDL_TimerID curr_timer;

	reg_t curr_interval;
	bool periodic;
} pit;

static Uint32 pit_callback(Uint32 interval, void *param)
{
	SDL_LockMutex(pit.mutex);

//	printf("pit_callback: interval %d\n", interval);

	// make sure there is still an active timer
	if (pit.curr_timer == NULL)
		goto exit;

	// edge trigger an interrupt
	pic_trigger_edge(INT_PIT);

	if (!pit.periodic) {
		// cancel the timer
		SDL_RemoveTimer(pit.curr_timer);
		pit.curr_timer = NULL;
	}

  exit:
	SDL_UnlockMutex(pit.mutex);

	return interval;
}

static word pit_regs_get_put(armaddr_t address, word data, int size, int put)
{
	word val = 0;

	SYS_TRACE(1, "sys: pit_regs_get_put at 0x%08x, data 0x%08x, size %d, put %d\n", 
		address, data, size, put);

	if(size < 4)
		return 0; /* only word accesses supported */

	SDL_LockMutex(pit.mutex);

	switch(address) {
	case PIT_STAT: // status bit
		break;
	case PIT_INTERVAL:
		if (put && data != 0) {
			pit.curr_interval = data;
		}
		val = pit.curr_interval;
		break;
	case PIT_START_ONESHOT:
		if (put && data != 0) {
			pit.periodic = FALSE;
			goto set_timer;
		}
		break;
	case PIT_START_PERIODIC:
		if (put && data != 0) {
			pit.periodic = TRUE;
			goto set_timer;
		}
		break;

  set_timer:
		// clear any old timer
		if (pit.curr_timer != NULL) {
			SDL_RemoveTimer(pit.curr_timer);
			pit.curr_timer = NULL;
		}
		pit.curr_timer = SDL_AddTimer(pit.curr_interval, &pit_callback, NULL);
		break;
	case PIT_CLEAR:
		if (put && data != 0 && pit.curr_timer != NULL) {
			SDL_RemoveTimer(pit.curr_timer);
			pit.curr_timer = NULL;
		}
		break;
	}

	SDL_UnlockMutex(pit.mutex);

	return val;
}

int initialize_pit(void)
{
	memset(&pit, 0, sizeof(pit));

	// create a mutex to lock us
	pit.mutex = SDL_CreateMutex();

	// install the pic register handlers
	install_mem_handler(PIT_REGS_BASE, PIT_REGS_SIZE, &pit_regs_get_put, NULL);

	return 0;
}
