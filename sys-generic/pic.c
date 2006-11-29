/*
 * Copyright (c) 2005-2006 Travis Geiselbrecht
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
	bool irq_active;
	uint32_t vector_active;    // 1 if active
	uint32_t vector_mask;      // 1 if the interrupt is masked
} pic;

/* set cpu irq status based off of current interrupt controller inputs */
static void set_irq_status(void)
{
	if (pic.vector_active & ~pic.vector_mask) {
		if (!pic.irq_active)
			raise_irq();
		pic.irq_active = TRUE;
	} else {
		if (pic.irq_active)
			lower_irq();
		pic.irq_active = FALSE;
	}
}

static int get_current_interrupt(void)
{
	int i;

	uint32_t ready_ints = pic.vector_active & ~pic.vector_mask;
	if (ready_ints == 0)
		return -1;

	for (i=0; i < PIC_MAX_INT; i++) {
		if (ready_ints & (1 << i))
			return i;
	}

	return -1;
}

int pic_assert_level(int vector)
{
	if(vector < 0 || vector >= PIC_MAX_INT)
		return -1;

	SDL_LockMutex(pic.mutex);

	SYS_TRACE(5, "sys: pic_assert_level %d\n", vector);

	pic.vector_active |= (1<<vector);
	set_irq_status();

	SDL_UnlockMutex(pic.mutex);

	return 0;
}

int pic_deassert_level(int vector)
{
	if(vector < 0 || vector >= PIC_MAX_INT)
		return -1;

	SDL_LockMutex(pic.mutex);

	SYS_TRACE(5, "sys: pic_deassert_level %d\n", vector);

	pic.vector_active &= ~(1<<vector);
	set_irq_status();

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
			set_irq_status();
		} else {
			val = pic.vector_mask;
		}
		break;

		/* each bit corresponds to the current status of the interrupt line */
	case PIC_STAT:
		val = pic.vector_active;
		break;

		/* one bit set for the highest priority non-masked active interrupt */
	case PIC_CURRENT_BIT: {
		int current_int = get_current_interrupt();

		val = (current_int >= 0) ? (1 << current_int) : 0;
		break;
	}

		/* holds the current interrupt number, check PIC_CURRENT_BIT to see if something is pending */
	case PIC_CURRENT_NUM: {
		int current_int = get_current_interrupt();
		val = (current_int >= 0) ? (word)current_int : 0xffffffff;
		break;
	}

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
