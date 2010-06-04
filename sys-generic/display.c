/*
 * Copyright (c) 2005-2010 Travis Geiselbrecht
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
#include <assert.h>

#include <SDL/SDL.h>
#include <SDL/SDL_thread.h>

#include <config.h>
#include <arm/arm.h>
#include <sys/sys.h>
#include "sys_p.h"
#include <util/endian.h>
#include <util/atomic.h>

#define DEFAULT_SCREEN_X 		640
#define DEFAULT_SCREEN_Y 		480
#define DEFAULT_SCREEN_DEPTH 	32

static struct display {
	// SDL surface structure
	SDL_Surface *screen;

	// geometry
	uint screen_x;
	uint screen_y;
	uint screen_depth;
	uint screen_size;
	
	// framebuffer backing store
	byte *fb;
	SDL_Surface *fbsurface;
	
	// dirty flag
	int dirty;
} display;

static word display_regs_get_put(armaddr_t address, word data, int size, int put)
{
	word ret;

	SYS_TRACE(5, "sys: display_regs_get_put at 0x%08x, data 0x%08x, size %d, put %d\n", 
		address, data, size, put);

	switch (address) {
		case DISPLAY_WIDTH:
			ret = display.screen_x;
			break;
		case DISPLAY_HEIGHT:
			ret = display.screen_y;
			break;
		case DISPLAY_BPP:
			ret = display.screen_depth;
			break;
		default:
			ret = 0;
	}


	return ret;
}

static word display_get_put(armaddr_t address, word data, int size, int put)
{
	byte *ptr;

	address -= DISPLAY_FRAMEBUFFER;

	if(address > DISPLAY_SIZE)
		SYS_TRACE(0, "sys: display_get_put with invalid address 0x%08x\n", address);

	ptr = display.fb + address;

	switch(size) {
		case 4:
			if(put)
				WRITE_MEM_WORD(ptr, data);
			else
				data = READ_MEM_WORD(ptr);
			break;
		case 2:
			if(put)
				WRITE_MEM_HALFWORD(ptr, data);
			else
				data = READ_MEM_HALFWORD(ptr);
			break;
		case 1:
			if(put)
				WRITE_MEM_BYTE(ptr, data);
			else
				data = READ_MEM_BYTE(ptr);
			break;
	}

	if(put)
		atomic_or(&display.dirty, 1);

	SYS_TRACE(6, "sys: display_get_put at 0x%08x, data 0x%08x, size %d, put %d\n", 
		address, data, size, put);

	return data;
}

// main display loop
static int display_thread_entry(void *args)
{
	word *src;
	word *dest;
	SDL_Surface *surface = display.screen;

	for(;;) {
		SDL_Delay(100);
	
		// is the surface dirty?
		if(atomic_set(&display.dirty, 0)) {			
			// redraw the entire window
			SDL_LockSurface(surface);
			
			src = (word *)display.fb;
			dest = (word *)surface->pixels;
			
			memcpy(dest, src, display.screen_size);
		
			SDL_UnlockSurface(surface);
			SDL_Flip(surface);
		}	
	}

	return 0;
}

int initialize_display(void)
{
	// initialize the SDL display
	if (SDL_InitSubSystem(SDL_INIT_VIDEO) < 0)
		return -1;

	memset(&display, 0, sizeof(display));

	// set up default geometry
	display.screen_x = DEFAULT_SCREEN_X;
	display.screen_y = DEFAULT_SCREEN_Y;
	display.screen_depth = DEFAULT_SCREEN_DEPTH;

	// see if any config variables override it
	const char *str;
	str = get_config_key_string("display", "width", NULL);
	if (str)
		display.screen_x = strtoul(str, NULL, 10);
	str = get_config_key_string("display", "height", NULL);
	if (str)
		display.screen_y = strtoul(str, NULL, 10);
	str = get_config_key_string("display", "depth", NULL);
	if (str)
		display.screen_depth = strtoul(str, NULL, 10);

	// sanity check geometry
	if (display.screen_x == 0 || display.screen_x > 4096) {
		SYS_TRACE(0, "sys: display width out of range %d\n", display.screen_x);
		return -1;
	}
	if (display.screen_y == 0 || display.screen_y > 4096) {
		SYS_TRACE(0, "sys: display height out of range %d\n", display.screen_y);
		return -1;
	}
	if (display.screen_depth != 16 && display.screen_depth != 32) {
		SYS_TRACE(0, "sys: invalid display depth %d\n", display.screen_depth);
		return -1;
	}

	// calculate size 
	display.screen_size = display.screen_x * display.screen_y * (display.screen_depth / 8);

	// create and register a memory range for the framebuffer
	display.fb = (byte *)calloc(DISPLAY_SIZE, 1);
	install_mem_handler(DISPLAY_BASE, DISPLAY_SIZE, &display_get_put, NULL);

	// install the display register handlers
	install_mem_handler(DISPLAY_REGS_BASE, DISPLAY_REGS_SIZE, &display_regs_get_put, NULL);

	// create the emulator window
	display.screen = SDL_SetVideoMode(display.screen_x, display.screen_y, display.screen_depth, SDL_HWSURFACE|SDL_DOUBLEBUF);
	if (!display.screen) {
		SYS_TRACE(0, "sys: error creating SDL surface\n");
		return -1;
	}
	
	SYS_TRACE(1, "created screen: w %d h %d pitch %d\n", display.screen->w, display.screen->h, display.screen->pitch);

	SDL_UpdateRect(display.screen, 0,0,0,0); // Update entire surface

	SDL_WM_SetCaption("ARMemu","ARMemu");

	// spawn a thread to deal with the display
	SDL_CreateThread(&display_thread_entry, NULL);

	return 0;
}
