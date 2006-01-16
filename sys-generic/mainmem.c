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

#include <arm/arm.h>
#include <sys/sys.h>
#include "sys_p.h"
#include <util/endian.h>

/* global system state */
struct mainmem {
	/* main memory backing store */
	byte *mem;
	armaddr_t base;
	armaddr_t size;
} mainmem;

static word mainmem_get_put(armaddr_t address, word data, int size, int put)
{
	void *ptr;

	ptr = mainmem.mem + (address - mainmem.base);

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

	SYS_TRACE(5, "sys: mainmem_get_put at 0x%08x, data 0x%08x, size %d, put %d\n", 
		address, data, size, put);

	return data;
}

static void *mainmem_get_ptr(armaddr_t address)
{
	return mainmem.mem + (address - mainmem.base);
}

int dump_mainmem(void)
{
	FILE *fp;

	fp = fopen("mainmem.bin", "w+");
	if(fp) {
		fwrite(mainmem.mem, mainmem.size, 1, fp);
		fclose(fp);
	}

	return 0;
}

int initialize_mainmem(const char *rom_file, long load_offset)
{
	// allocate some ram
	mainmem.size = MAINMEM_SIZE;
	mainmem.base = MAINMEM_BASE;
	mainmem.mem = calloc(1, mainmem.size);	

	printf("sys: initializing mainmem from rom file %s, offset %ld\n", rom_file, load_offset);

	// put it in the memory map
	install_mem_handler(mainmem.base, mainmem.size, &mainmem_get_put, &mainmem_get_ptr);

	// read in a file, if specified
	if(rom_file) {
		FILE *fp = fopen(rom_file, "r");
		if(fp) {
			fread(mainmem.mem + load_offset, 1, mainmem.size, fp);
			fclose(fp);
		}
	}

	return 0;
}

