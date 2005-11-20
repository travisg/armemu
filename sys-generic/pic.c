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

static struct pic {
	SDL_mutex *mutex;
	int active;
	int curr_int;
	uint32_t vector_active;    // 1 if active
	uint32_t vector_edge;      // 1 for edge, 0 for level
	uint32_t vector_mask;      // 1 if the interrupt is masked
} pic;

static void go_active(int vector)
{
//	printf("go_active: vector %d\n", vector);

	pic.active = 1;
	pic.curr_int = vector;
	raise_irq();
}

static void go_inactive(void)
{
//	printf("go_inactive\n");

	pic.active = 0;
	pic.curr_int = 0;
	lower_irq();
}

/* check to see if we need to go active after setting a new vector active */
static int check_for_active_after_assert(int new_vector)
{
	if(pic.active)
		return 0; /* we're already active */

	/* if it's not masked out, make the new active vector the newly asserted vector */
	if(BIT(pic.vector_active & ~pic.vector_mask, new_vector)) {
		go_active(new_vector);
		return 1;
	}

	return 0;
}

/* check to see if we need to go inactive */
static int check_for_inactive(void)
{
	if(!pic.active)
		return 0; /* we're already inactive */

	/* if nothing is asserted, go inactive */
	if((pic.vector_active & ~pic.vector_mask) == 0) {
		go_inactive();
		return 1;
	}

	return 0;
}

/* check to see if we need to change state after the mask was updated */
static void check_after_mask_change(void)
{
	int i;

	if(pic.active) {
		/* if we're already active, see if the new mask masks out our active vector */	
		// XXX what should happen here? should we change the currently active vector?
	} else {
		/* see if a newly unmasked vector makes the system go active */
		uint32_t ready_vectors = pic.vector_active & ~pic.vector_mask;
		if(ready_vectors != 0) { 
			for(i=0; i < PIC_MAX_INT; i++) {
				if(ready_vectors & (1<<i)) {
					/* we're going active */
					go_active(i);
					break;
				}
			}
		}
	}
}

static void deassert_edge(uint32_t mask)
{
//	printf("deassert_edge: mask 0x%x, pic.vector_edge 0x%x, pic.vector_active 0x%x, pic.active %d, pic.curr_int %d\n", 
//		   mask, pic.vector_edge, pic.vector_active, pic.active, pic.curr_int);

	/* deassert all active edge interrupts covered by this mask */
	mask &= pic.vector_edge;
	pic.vector_active &= ~mask;

	/* now see if the active vector just got cleared, and if so
	 * find a new active vector.
	 */
	if(pic.active) {
		if(BIT(pic.vector_active, pic.curr_int) == 0) {
			/* we need a new active vector */
			if(check_for_inactive())
				return; /* we went inactive because there was nothing else ready to run */

			/* we need to search for the next active vector */
			int i;
			for(i = 0; i < PIC_MAX_INT; i++) {
				if(BIT(pic.vector_active & ~pic.vector_mask, i)) {
					go_active(i);
					return;
				}
			}
			panic_cpu("pic: deassert_edge() in bad state\n");
		}
	}

}

int pic_trigger_edge(int vector)
{
	if(vector < 0 || vector >= PIC_MAX_INT)
		return -1;

	SDL_LockMutex(pic.mutex);

	pic.vector_active |= (1<<vector);
	pic.vector_edge |= (1<<vector);
	check_for_active_after_assert(vector);

	SDL_UnlockMutex(pic.mutex);

	return 0;
}

int pic_assert_level(int vector)
{
	if(vector < 0 || vector >= PIC_MAX_INT)
		return -1;

	SDL_LockMutex(pic.mutex);

	pic.vector_active |= (1<<vector);
	pic.vector_edge &= ~(1<<vector);
	check_for_active_after_assert(vector);

	SDL_UnlockMutex(pic.mutex);

	return 0;
}

int pic_deassert_level(int vector)
{
	if(vector < 0 || vector >= PIC_MAX_INT)
		return -1;

	SDL_LockMutex(pic.mutex);

	pic.vector_active &= ~(1<<vector);
	pic.vector_edge &= ~(1<<vector);
	check_for_inactive();

	SDL_UnlockMutex(pic.mutex);

	return 0;
}

static word pic_regs_get_put(armaddr_t address, word data, int size, int put)
{
	word val;

	SYS_TRACE(5, "sys: pic_regs_get_put at 0x%08x, data 0x%08x, size %d, put %d\n", 
		address, data, size, put);

	if(size < 4)
		return 0; /* only word accesses supported */

	SDL_LockMutex(pic.mutex);

	switch(address) {
		/* read/write to the current interrupt mask */
	case PIC_MASK_LATCH: /* 1s are latched into the current mask */
		data |= pic.vector_mask;
		goto set_mask;
	case PIC_UNMASK_LATCH: /* 1s are latched as 0s in the current mask */
		data = pic.vector_mask & ~data;
set_mask:
	case PIC_MASK:
		if(put) {
			pic.vector_mask = data;
			val = 0;
			check_after_mask_change();
		} else {
			val = pic.vector_mask;
		}
		break;

		/* each bit corresponds to the current status of the interrupt line */
	case PIC_STAT:
		val = pic.vector_active;
		break;

		/* one bit set for the current interrupt. */
		/* write one to any bit to clear its status if it's edge triggered. */
	case PIC_CURRENT_BIT:
		if(put) {
			/* deassert any edge interrupt by writing 1s to this register */
			deassert_edge(data);
			val = 0;
		} else {
			/* read the current interrupt as a bit set. curr_int is only valid when we're active */
			if(pic.active) {
				val = (1 << pic.curr_int);
			} else {
				val = 0;
			}
		}
		break;

		/* holds the current interrupt number, check PIC_CURRENT_BIT to see if something is pending */
	case PIC_CURRENT_NUM:
		val = pic.curr_int;
		break;

	default:
		val = 0;
	}

	SDL_UnlockMutex(pic.mutex);

	return val;
}

int initialize_pic(void)
{
	memset(&pic, 0, sizeof(pic));

	// create a mutex to lock us
	pic.mutex = SDL_CreateMutex();

//	pic.vector_mask = 0xffffffff; /* everything starts out masked */

	// install the pic register handlers
	install_mem_handler(PIC_REGS_BASE, PIC_REGS_SIZE, &pic_regs_get_put, NULL);

	return 0;
}
