#ifndef __SYS_P_H
#define __SYS_P_H

#include <SDL/SDL.h>

// main memory
int dump_mainmem(void);
int initialize_mainmem(const char *binary_file, const char *rom_file);

// interrupt controller
int initialize_pic(void);
int pic_trigger_edge(int vector);   /* edge triggered interrupts */
int pic_assert_level(int vector);   /* level triggered interrupts use these */
int pic_deassert_level(int vector);

// display
int initialize_display(void);

// console
int initialize_console(void);
void console_keydown(SDLKey key);
void console_keyup(SDLKey key);

// memory map
#include "memmap.h"

#endif
