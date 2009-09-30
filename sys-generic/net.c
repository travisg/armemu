/*
 * Copyright (c) 2005-2008 Travis Geiselbrecht
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

#if WITH_TUNTAP
/* tuntap stuff */
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <linux/if.h>
#include <linux/if_tun.h>

#define PACKET_LEN 2048
#define PACKET_QUEUE_LEN 32	/* must be power of 2 */

static struct network {
	int fd;

	int head;
	int tail;

	uint out_packet_len;
	uint8_t out_packet[PACKET_LEN];
	uint in_packet_len[PACKET_QUEUE_LEN];
	uint8_t in_packet[PACKET_QUEUE_LEN][PACKET_LEN];
} *network;

static word buffer_read(const void *_ptr, uint offset, int size)
{
	const uint8_t *ptr = (const uint8_t *)_ptr;

	switch (size) {
		default:
		case 1:
			return ptr[offset];
			break;
		case 2:
			return *(uint16_t *)&ptr[offset];
			break;
		case 4:
			return *(uint32_t *)&ptr[offset];
			break;
	}
}

static word buffer_write(void *_ptr, uint offset, word data, int size)
{
	uint8_t *ptr = (uint8_t *)_ptr;

	switch (size) {
		default:
		case 1:
			ptr[offset] = data;
			break;
		case 2:
			*(uint16_t *)&ptr[offset] = data;
			break;
		case 4:
			*(uint32_t *)&ptr[offset] = data;
			break;
	}

	return data;
}

static word network_regs_get_put(armaddr_t address, word data, int size, int put)
{
	word val;
	uint offset;

	SYS_TRACE(5, "sys: network_regs_get_put at 0x%08x, data 0x%08x, size %d, put %d\n", 
		address, data, size, put);

	switch (address) {
		case NET_HEAD:
			/* head is read/only */
			val = network->head;
			break;
		case NET_TAIL:
			if (put) {
				network->tail = data % PACKET_QUEUE_LEN;
				if (network->head == network->tail) {
					pic_deassert_level(INT_NET);
				}
			}
			val = network->tail;
			break;
		case NET_SEND:
			if (put) {
				write(network->fd, network->out_packet, network->out_packet_len);
			}
			val = 0;
			break;
		case NET_SEND_LEN:
			if (put) {
				network->out_packet_len = data % PACKET_LEN;
			}
			val = network->out_packet_len;
			break;
		case NET_IN_BUF_LEN:
			/* read/only */
			val = network->in_packet_len[network->tail];
			break;
		case NET_OUT_BUF...(NET_OUT_BUF + NET_BUF_LEN - 1):
			offset = address - NET_OUT_BUF;
			if (put) {
				val = buffer_write(network->out_packet, offset, data, size);
			} else {
				val = buffer_read(network->out_packet, offset, size);
			}
			break;
		case NET_IN_BUF...(NET_IN_BUF + NET_BUF_LEN - 1):
			offset = address - NET_IN_BUF;

			/* in buffers are read/only */
			val = buffer_read(network->in_packet[network->tail], offset, size);
			break;
		default:
			SYS_TRACE(0, "sys: unhandled network address 0x%08x\n", address);
			return 0;
	}

	return val;
}

static int network_thread(void *args)
{
	for (;;) {
		ssize_t ret;
		ret = read(network->fd, network->in_packet[network->head], PACKET_LEN);
		if (ret > 0) {
			SYS_TRACE(2, "sys: got network data, size %d, head %d, tail %d\n", ret, network->head, network->tail);

			network->in_packet_len[network->head] = ret;

			network->head = (network->head + 1) % PACKET_QUEUE_LEN;
			pic_assert_level(INT_NET);
		}
	}

	return 0;
}

static int open_tun(const char *dev)
{
	struct ifreq ifr;
	int fd, err;

	fd = open("/dev/net/tun", O_RDWR);
	if (fd < 0)
		return fd;

	memset(&ifr, 0, sizeof(ifr));

	printf("fd %d\n", fd);

	/* Flags: IFF_TUN   - TUN device (no Ethernet headers)
	*        IFF_TAP   - TAP device
	*
	*        IFF_NO_PI - Do not provide packet information
	*/
	ifr.ifr_flags = IFF_TAP;
	if( *dev )
		strncpy(ifr.ifr_name, dev, IFNAMSIZ);

	if( (err = ioctl(fd, TUNSETIFF, (void *) &ifr)) < 0 ){
		close(fd);
		return err;
	}
	return fd;
}
#endif // WITH_TUNTAP

int initialize_network(void)
{
	const char *str;

#if WITH_TUNTAP
	network = calloc(sizeof(*network), 1);

	// install the network register handlers
	install_mem_handler(NET_REGS_BASE, NET_REGS_SIZE, &network_regs_get_put, NULL);

	// try to intialize the tun/tap interface
	str = get_config_key_string("network", "device", NULL);
	if (!str) {
		SYS_TRACE(0, "sys: no network device node specified in config file, aborting.\n");
		exit(1);
	}

	// try to open the device
	network->fd = open_tun(str);
	if (network->fd < 0) {
		SYS_TRACE(0, "sys: failed to open tun/tap interface at '%s'\n", str);
		exit(1);
	}

	// start a network reader/writer thread
	SDL_CreateThread(&network_thread, NULL);
#endif

	return 0;		
}

