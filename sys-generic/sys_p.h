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
#ifndef __SYS_P_H
#define __SYS_P_H

#include <SDL/SDL.h>

// main memory
int dump_mainmem(void);
int initialize_mainmem(const char *rom_file, long load_offset);

// interrupt controller
int initialize_pic(void);
int pic_assert_level(int vector);   /* level triggered interrupts use these */
int pic_deassert_level(int vector);

// timer
int initialize_pit(void);

// display
int initialize_display(void);

// console
int initialize_console(void);
void console_keydown(SDLKey key);
void console_keyup(SDLKey key);

// network
int initialize_network(void);

// block device
int initialize_blockdev(void);

// debug  
int initialize_debug(void);

// memory map
#include "memmap.h"

#endif
