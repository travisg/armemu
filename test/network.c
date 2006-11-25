#include "network.h"
#include "debug.h"
#include "memmap.h"

static const void *index_to_inbuf(unsigned int index)
{
	return (const void *)(NET_IN_BUFS + NET_BUF_LEN * index);
}

void network_int_handler(void)
{
	int tail;
	void *inbuf;

	puts("network int handler\n");

	tail = *REG(NET_TAIL);
	debug_dump_memory_words(&tail, 1);

	/* calculate the in buffer */
	debug_dump_memory_words(index_to_inbuf(tail), 64);


	*REG(NET_TAIL) = *REG(NET_HEAD);
}

