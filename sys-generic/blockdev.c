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
#include <fcntl.h>
#include <unistd.h>

#include <SDL/SDL.h>
#include <SDL/SDL_thread.h>

#include <config.h>
#include <arm/arm.h>
#include <sys/sys.h>
#include "sys_p.h"
#include <util/endian.h>
#include <util/atomic.h>

static struct bdev {
	int fd;

	off_t length;

	// pending command stuff
	uint cmd;
	armaddr_t trans_addr;
	off_t trans_off;
	size_t trans_len;

	// error codes
	uint last_err;
} *bdev;

static uint bdev_read(armaddr_t address, off_t offset, size_t length)
{
	SYS_TRACE(1, "sys: bdev_read at 0x%08x, offset 0x%16llx, size %zd\n", 
		address, offset, length);

	byte buf[512];

	lseek(bdev->fd, offset, SEEK_SET);
	while (length > 0) {
		size_t tohandle = MIN(sizeof(buf), length);

		ssize_t err = read(bdev->fd, buf, tohandle);
		if (err != (ssize_t)tohandle)
			return BDEV_CMD_ERR_GENERAL;

		size_t i;
		for (i = 0; i < tohandle / 4; i++) {
			sys_write_mem_word(address + i*4, *(word *)(&buf[i * 4]));
		}
		
		for (; i < tohandle; i++)
			sys_write_mem_byte(address + i, *(word *)(&buf[i]));

		length -= tohandle;
		address += tohandle;
	}

	return BDEV_CMD_ERR_NONE;
}

static uint bdev_write(armaddr_t address, off_t offset, size_t length)
{
	SYS_TRACE(5, "sys: bdev_write at 0x%08x, offset 0x%16llx, size %zd\n", 
		address, offset, length);

	byte buf[512];

	lseek(bdev->fd, offset, SEEK_SET);
	while (length > 0) {
		size_t tohandle = MIN(sizeof(buf), length);

		size_t i;
		for (i = 0; i < tohandle / 4; i++) {
			*(word *)(&buf[i * 4]) = sys_read_mem_word(address + i*4);
		}
		
		for (; i < tohandle; i++)
			*(word *)(&buf[i]) = sys_read_mem_byte(address + i);

		ssize_t err = write(bdev->fd, buf, tohandle);
		if (err != (ssize_t)tohandle)
			return BDEV_CMD_ERR_GENERAL;

		length -= tohandle;
		address += tohandle;
	}

	return BDEV_CMD_ERR_NONE;
}

static uint bdev_erase(off_t offset, size_t length)
{
	SYS_TRACE(5, "sys: bdev_erase offset 0x%16llx, size %zd\n", 
		offset, length);

	char buf[512];
	memset(buf, 0, sizeof(buf));

	lseek(bdev->fd, offset, SEEK_SET);
	while (length > 0) {
		size_t towrite = MIN(sizeof(buf), length);

		int err = write(bdev->fd, buf, towrite);
		if (err != 0)
			return BDEV_CMD_ERR_GENERAL;

		length -= towrite;
	}

	return BDEV_CMD_ERR_NONE;
}

static word bdev_regs_get_put(armaddr_t address, word data, int size, int put)
{
	word val;

	SYS_TRACE(5, "sys: bdev_regs_get_put at 0x%08x, data 0x%08x, size %d, put %d\n", 
		address, data, size, put);

	val = 0;
	switch (address) {
		case BDEV_CMD:
			if (put) {
				/* mask out the command portion of the write */
				data &= BDEV_CMD_MASK;

				switch (data) {
					case BDEV_CMD_READ:
						bdev->last_err = bdev_read(bdev->trans_addr, bdev->trans_off, bdev->trans_len);
						break;
					case BDEV_CMD_WRITE:
						bdev->last_err = bdev_write(bdev->trans_addr, bdev->trans_off, bdev->trans_len);
						break;
					case BDEV_CMD_ERASE:
						bdev->last_err = bdev_erase(bdev->trans_off, bdev->trans_len);
						break;
				}
			} else {
				// read last error
				val = (bdev->last_err << BDEV_CMD_ERRSHIFT) | bdev->cmd;
			}
			break;
		case BDEV_CMD_ADDR:
			if (put) {
				bdev->trans_addr = data;
			} else {
				val = bdev->trans_addr;
			}
			break;
		case BDEV_CMD_OFF:
			if (put) {
				bdev->trans_off = (bdev->trans_off & 0xffffffff00000000ULL) | data;
			} else {
				val = (bdev->trans_off & 0xffffffff);
			}
			break;
		case BDEV_CMD_OFF + 4:
			if (put) {
				bdev->trans_off = (bdev->trans_off & 0xffffffff) | ((off_t)data << 32);
			} else {
				val = (bdev->trans_off >> 32);
			}
			break;
		case BDEV_CMD_LEN:
			if (put) {
				bdev->trans_len = data;
			} else {
				val = bdev->trans_len;
			}
			break;
		case BDEV_LEN:
			if (!put) {
				val = (bdev->length & 0xffffffff);
			}
			break;
		case BDEV_LEN + 4:
			if (!put) {
				val = (bdev->length >> 32);
			}
			break;

		default:
			SYS_TRACE(0, "sys: unhandled bdev address 0x%08x\n", address);
			return 0;
	}

	return val;
}

int initialize_blockdev(void)
{
	const char *str;

	bdev = calloc(sizeof(*bdev), 1);

	install_mem_handler(BDEV_REGS_BASE, BDEV_REGS_SIZE, &bdev_regs_get_put, NULL);

	str = get_config_key_string("block", "file", "");
	printf("str '%s'\n", str);

	bdev->fd = -1;
	if (strlen(str) > 0) {
		bdev->fd = open(str, O_RDWR);
		bdev->length = 1024*1024; // XXX get stat
		printf("fd %d\n", bdev->fd);
	}

	return 0;		
}

