#include "block.h"
#include "memmap.h"

int block_is_present(void)
{
	return (*REG(SYSINFO_FEATURES) & SYSINFO_FEATURE_BLOCKDEV) ? 1 : 0;
}

int block_get_params(unsigned long long *size)
{
	if (!block_is_present())
		return -1;

	*size = (*REG64(BDEV_LEN));

	return 0;
}

static int block_read_write(unsigned long long offset, unsigned int len, void *buf, int readwrite)
{
//	*REG(DEBUG_SET_TRACELEVEL_SYS) = 4;
	*REG(BDEV_CMD_ADDR) = (unsigned int)buf;
	*REG64(BDEV_CMD_OFF) = offset;
	*REG(BDEV_CMD_LEN) = len;

	if (readwrite)
		*REG(BDEV_CMD) = BDEV_CMD_READ;
	else
		*REG(BDEV_CMD) = BDEV_CMD_WRITE;
//	*REG(DEBUG_SET_TRACELEVEL_SYS) = 0;

	return 0;
}

int block_read(unsigned long long offset, unsigned int len, void *buf)
{
	return block_read_write(offset, len, buf, 1);
}

int block_write(unsigned long long offset, unsigned int len, const void *buf)
{
	return block_read_write(offset, len, (void *)buf, 0);
}

