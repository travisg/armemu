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

#define KB_BUFSIZE 1024

static struct console {
	// XXX implement keyboard buffer
	int head;
	int tail;
	unsigned int keybuffer[1024];
} console;

enum {
	KEY_MOD_LSHIFT = 0x00010000,
	KEY_MOD_RSHIFT = 0x00020000,
	KEY_MOD_LCTRL  = 0x00040000,
	KEY_MOD_RCTRL  = 0x00080000,
	KEY_MOD_LALT   = 0x00100000,
	KEY_MOD_RALT   = 0x00200000,
	KEY_MOD_LMETA  = 0x00400000,
	KEY_MOD_RMETA  = 0x00800000,
	KEY_MOD_NUM    = 0x01000000,
	KEY_MOD_CAPS   = 0x02000000,
	KEY_MOD_MODE   = 0x04000000,
	KEY_MOD_UP     = 0x80000000,
	KEY_MOD_MASK   = 0xffff0000,
};

#define IS_BUF_FULL (((console.tail - 1) % KB_BUFSIZE) == console.head)
#define IS_BUF_EMPTY (console.tail == console.head)
#define USED_BUF_ENTRIES ((console.head - console.tail) % KB_BUFSIZE)
#define REMAING_BUF_ENTRIES (KB_BUFSIZE - USED_BUF_ENTRIES)

static void insert_key(SDLKey key)
{
	/* for now, just stuff the raw sdl key into the buffer */
	if(!IS_BUF_FULL) {
		console.keybuffer[console.head] = key;
		console.head = (console.head + 1) % KB_BUFSIZE;
	}

	/* assert an interrupt */
	pic_assert_level(INT_KEYBOARD);
}

static unsigned int read_key(void)
{
	unsigned int key = 0;

	if(!IS_BUF_EMPTY) {
		key = console.keybuffer[console.tail];
		console.tail = (console.tail + 1) % KB_BUFSIZE;
	}

	/* if the buffer is now empty, deassert interrupt */
	if(IS_BUF_EMPTY)
		pic_deassert_level(INT_KEYBOARD);

	return key;
}

static word console_regs_get_put(armaddr_t address, word data, int size, int put)
{
	word val;

	SYS_TRACE(5, "sys: console_regs_get_put at 0x%08x, data 0x%08x, size %d, put %d\n", 
		address, data, size, put);

	if(put)
		return 0; /* can't write to the keyboard */

	switch(address) {
		case KYBD_STAT: /* status register */
			val = IS_BUF_EMPTY ? 0 : 1; /* we're not empty */
			break;
		case KYBD_DATA: /* keyboard data */
			val = read_key();
			break;
		default:
			SYS_TRACE(0, "unhandled keyboard address 0x%08x\n", address);
			return 0;
	}

	return val;
}

void console_keydown(SDLKey key)
{
//	printf("console_keydown: key 0x%x\n", key);

	insert_key(key);
}

void console_keyup(SDLKey key)
{
//	printf("console_keyup: key 0x%x\n", key);

	insert_key(key | KEY_MOD_UP);
}

int initialize_console(void)
{
	memset(&console, 0, sizeof(console));

	// install the console register handlers
	install_mem_handler(CONSOLE_REGS_BASE, CONSOLE_REGS_SIZE, &console_regs_get_put, NULL);

	return 0;
}
