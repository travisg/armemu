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
#include <sys/time.h>

#include <SDL/SDL.h>

#include <config.h>
#include <arm/arm.h>
#include <sys/sys.h>
#include "sys_p.h"

typedef struct _memory_map {
	word (*get_put)(armaddr_t address, word data, int size, int put);
	void* (*get_ptr)(armaddr_t address);
} memory_map;

#define MEMORY_BANK_SHIFT 22	 // 4MB
#define MEMORY_BANK_SIZE (1 << MEMORY_BANK_SHIFT)
#define ADDR_TO_BANK(x) ((x) >> MEMORY_BANK_SHIFT) 
#define MEMORY_BANK_COUNT (1<<(32-MEMORY_BANK_SHIFT))

/* global system state */
struct sys {
	/* features */
	uint features;

	/* current latched system time */
	struct timeval current_time;

	/* main memory map */
	memory_map memmap[MEMORY_BANK_COUNT];
} sys;

// function decls
static word unhandled_get_put(armaddr_t address, word data, int size, int put);
static int initialize_sysinfo_regs(void);

static bool has_sys_feature(const char *name, bool def)
{
	return get_config_key_bool("system", name, def);
}
    
static void load_feature_config(void)
{
	sys.features = 0;

	// load system feature config
	sys.features |= has_sys_feature("console", FALSE) ? SYSINFO_FEATURE_CONSOLE : 0;
	sys.features |= has_sys_feature("display", FALSE) ? SYSINFO_FEATURE_DISPLAY : 0;
	sys.features |= has_sys_feature("network", FALSE) ? SYSINFO_FEATURE_NETWORK : 0;
	sys.features |= has_sys_feature("block", FALSE) ? SYSINFO_FEATURE_BLOCKDEV : 0;
}

int initialize_system(void)
{
	unsigned int i;
	
	// create a cpu
	initialize_cpu(get_config_key_string("cpu", "core", "arm7tdmi"));

	memset(&sys, 0, sizeof(sys));

	// load the feature set
	load_feature_config();
	
	// create the default memory map
	for(i=0; i < MEMORY_BANK_COUNT; i++)
		sys.memmap[i].get_put = &unhandled_get_put;

	// add the sysinfo registers
	initialize_sysinfo_regs();

	// initialize the interrupt controller
	initialize_pic();

	// initialize the timer
	initialize_pit();

	// initialize the main memory
	initialize_mainmem(get_config_key_string("rom", "file", NULL), 
			atol(get_config_key_string("rom", "address", "0")));

    if (sys.features & SYSINFO_FEATURE_DISPLAY){
            // initialize the display
        initialize_display();
    }

    if (sys.features & SYSINFO_FEATURE_CONSOLE){
            // initialize the console (keyboard)
        initialize_console();
    }

    if (sys.features & SYSINFO_FEATURE_NETWORK){
            // initialize the network (via tun/tap)
        initialize_network();
    }

    if (sys.features & SYSINFO_FEATURE_BLOCKDEV){
            // initialize the block device
        initialize_blockdev();
    }
// debug device
    initialize_debug();
    
	return 0;
}

void install_mem_handler(armaddr_t base, armaddr_t len,
	word (*get_put)(armaddr_t address, word data, int size, int put),
	void* (*get_ptr)(armaddr_t address))
{
	unsigned int i;

	SYS_TRACE(5, "install_mem_handler: base 0x%08x, len 0x%08x, get_put %p, get_ptr %p\n", base, len, get_put, get_ptr);

	// put it in the memory map
	for(i = ADDR_TO_BANK(base); i <= ADDR_TO_BANK(base + (len - 1)); i++) {
		sys.memmap[i].get_put = get_put;
		sys.memmap[i].get_ptr = get_ptr;
	}
}

void system_reset(void)
{
	reset_cpu();
}

void system_start(void)
{
	system_reset();
	start_cpu();
}

int system_message_loop(void)
{

	if (sys.features & SYSINFO_FEATURE_DISPLAY) {
		SDL_Event event;
		int quit = 0;
		while(!quit) {
			SDL_WaitEvent(&event);

			switch (event.type) {
				case SDL_KEYDOWN:
	//				printf("The %s 0x%x key was pressed!\n", 
	//					SDL_GetKeyName(event.key.keysym.sym), event.key.keysym.sym);
					console_keydown(event.key.keysym.sym);
					break;
				case SDL_KEYUP:
	//				printf("The %s 0x%x key was released!\n", 
	//					SDL_GetKeyName(event.key.keysym.sym), event.key.keysym.sym);
					console_keyup(event.key.keysym.sym);
					break;
				case SDL_QUIT:
					quit = 1;
					break;
			}
		}
	} else {
		// we don't have a display, and thus cannot have an event loop
		
		// wait here for ^c
		for(;;)
			sleep(10000);
	}

	return 0;
}

void dump_sys(void)
{
	printf("dumping system state...\n");

	dump_mainmem();

}

static word unhandled_get_put(armaddr_t address, word data, int size, int put)
{
	SYS_TRACE(1, "sys: unhandled get_put at 0x%08x, data 0x%08x, size %d, put %d\n", 
		address, data, size, put);
	panic_cpu("unhandled memory\n");
	return 0;
}

word sys_read_mem_word(armaddr_t address)
{
	return sys.memmap[ADDR_TO_BANK(address)].get_put(address, 0, 4, 0);
}

halfword sys_read_mem_halfword(armaddr_t address)
{
	return sys.memmap[ADDR_TO_BANK(address)].get_put(address, 0, 2, 0);
}

byte sys_read_mem_byte(armaddr_t address)
{
	return sys.memmap[ADDR_TO_BANK(address)].get_put(address, 0, 1, 0);
}

void sys_write_mem_word(armaddr_t address, word data)
{
	sys.memmap[ADDR_TO_BANK(address)].get_put(address, data, 4, 1);
}

void sys_write_mem_halfword(armaddr_t address, halfword data)
{
	sys.memmap[ADDR_TO_BANK(address)].get_put(address, data, 2, 1);
}

void sys_write_mem_byte(armaddr_t address, byte data)
{
	sys.memmap[ADDR_TO_BANK(address)].get_put(address, data, 1, 1);
}

void *sys_get_mem_ptr(armaddr_t address)
{
	if(sys.memmap[ADDR_TO_BANK(address)].get_ptr == NULL)
		return NULL;
	return sys.memmap[ADDR_TO_BANK(address)].get_ptr(address);
}

/* sysinfo register handlers */

static word sysinfo_get_put(armaddr_t address, word data, int size, int put)
{
	switch(address) {
	case SYSINFO_FEATURES:
		return sys.features;
	case SYSINFO_TIME_LATCH:
		if (put) {
			gettimeofday(&sys.current_time, NULL);
		}
		break;
	case SYSINFO_TIME_SECS:
		return sys.current_time.tv_sec;
	case SYSINFO_TIME_USECS:
		return sys.current_time.tv_usec;
	}

	return 0;
}

static int initialize_sysinfo_regs(void)
{
    install_mem_handler(SYSINFO_REGS_BASE, SYSINFO_REGS_SIZE,
                        sysinfo_get_put, NULL);

	return 0;
}
