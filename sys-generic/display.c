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
#include "sys_p.h"
#include <util/endian.h>
#include <util/atomic.h>

#define SCREEN_X 		640
#define SCREEN_Y 		480
#define SCREEN_DEPTH 	32

static struct display {
	// SDL surface structure
	SDL_Surface *screen;
	
	// framebuffer backing store
	byte *fb;
	
	// dirty flag
	int dirty;
} display;

static word display_regs_get_put(armaddr_t address, word data, int size, int put)
{
	SYS_TRACE(5, "sys: display_regs_get_put at 0x%08x, data 0x%08x, size %d, put %d\n", 
		address, data, size, put);

	// XXX implement some control regs (x, y, commands, etc)

	return 0;
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

	SYS_TRACE(5, "sys: display_get_put at 0x%08x, data 0x%08x, size %d, put %d\n", 
		address, data, size, put);

	return data;
}

// main display loop
static int display_thread_entry(void *args)
{
	int x, y;
	word *src;
	word *dest;
	int pitch;
	SDL_Surface *surface = display.screen;

	for(;;) {
		SDL_Delay(50);
	
		// is the surface dirty?
		if(atomic_set(&display.dirty, 0)) {			
			// redraw the entire window
			SDL_LockSurface(surface);
			
			src = (word *)display.fb;
			dest = (word *)surface->pixels;
			pitch = surface->pitch / sizeof(word);
			for(y = 0; y < SCREEN_Y; y++) {
				for(x = 0; x < SCREEN_X; x++) {
					*dest = READ_MEM_WORD(src);
					src++;
					dest++;
				}
				dest += pitch - SCREEN_X; // the SDL surface may have additional dead pixels at the end of the line
			}
		
			SDL_UnlockSurface(surface);
			SDL_Flip(surface);
		}	
	}
}

int initialize_display(void)
{
	memset(&display, 0, sizeof(display));

	// create and register a memory range for the framebuffer
	display.fb = (byte *)calloc(DISPLAY_SIZE, 1);
	install_mem_handler(DISPLAY_BASE, DISPLAY_SIZE, &display_get_put, NULL);

	// install the display register handlers
	install_mem_handler(DISPLAY_REGS_BASE, DISPLAY_REGS_SIZE, &display_regs_get_put, NULL);

	// create the emulator window
	display.screen = SDL_SetVideoMode(SCREEN_X, SCREEN_Y, SCREEN_DEPTH, SDL_HWSURFACE|SDL_DOUBLEBUF);
	
	SYS_TRACE(1, "created screen: w %d h %d pitch %d\n", display.screen->w, display.screen->h, display.screen->pitch);

	SDL_UpdateRect(display.screen, 0,0,0,0); // Update entire surface

	SDL_WM_SetCaption("ARMemu","ARMemu");

	// spawn a thread to deal with the display
	SDL_CreateThread(&display_thread_entry, NULL);

	return 0;
}
