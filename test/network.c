#include "network.h"
#include "debug.h"
#include "memmap.h"

void network_int_handler(void)
{
	int tail;
	unsigned int len;
	void *inbuf;

	tail = *REG(NET_TAIL);
	debug_dump_memory_words(&tail, 1);

	len = *REG(NET_IN_BUF_LEN);
	debug_dump_memory_words(&len, 1);

	/* calculate the in buffer */
	debug_dump_memory_words((void *)NET_IN_BUF, (len + 3) / 4);

	*REG(NET_TAIL) = *REG(NET_HEAD);
}

