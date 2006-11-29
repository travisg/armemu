#include "network.h"
#include "debug.h"
#include "memmap.h"

static unsigned char netbuf[NET_BUF_LEN];

void send_packet(const unsigned char *buf, unsigned int len)
{
	int i;
	unsigned long crc;

	for (i=0; i < len; i++)
		((volatile unsigned char *)NET_OUT_BUF)[i] = buf[i];

	*REG(NET_SEND_LEN) = len;
	*REG(NET_SEND) = 1;
}

void network_int_handler(void)
{
	unsigned int tail;
	unsigned int head;
	unsigned int len;
	int i;
	void *inbuf;

	head = *REG(NET_HEAD);
	tail = *REG(NET_TAIL);

	while (tail != head) {
		len = *REG(NET_IN_BUF_LEN);

		for (i=0; i < len; i++)
			netbuf[i] = ((unsigned char *)NET_IN_BUF)[i];

		debug_dump_memory_bytes(netbuf, len);

		send_packet(netbuf, len);

		tail = (tail + 1) % NET_IN_BUF_COUNT;
	}
	*REG(NET_TAIL) = tail;
}

