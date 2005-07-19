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
#ifndef __SYS_H
#define __SYS_H

#include <systypes.h>
#include <arm/arm.h>

int initialize_system(const char *binfile, const char *romfile, const char *cpu_type);
void system_reset(void);
void system_start(int cycles);
int system_message_loop(void);
void install_mem_handler(armaddr_t base, armaddr_t len,
	word (*get_put)(armaddr_t address, word data, int size, int put),
	void* (*get_ptr)(armaddr_t address));
void dump_sys(void);

/* referenced by the cpu */
word sys_read_mem_word(armaddr_t address);
halfword sys_read_mem_halfword(armaddr_t address);
byte sys_read_mem_byte(armaddr_t address);
void sys_write_mem_word(armaddr_t address, word data);
void sys_write_mem_halfword(armaddr_t address, halfword data);
void sys_write_mem_byte(armaddr_t address, byte data);

void *sys_get_mem_ptr(armaddr_t address);

#endif
