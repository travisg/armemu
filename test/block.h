#ifndef __BLOCK_H
#define __BLOCK_H

int block_is_present(void);
int block_get_params(unsigned long long *size);
int block_read(unsigned long long offset, unsigned int len, void *buf);
int block_write(unsigned long long offset, unsigned int len, const void *buf);

#endif

