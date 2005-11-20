#ifndef __DEBUG_H
#define __DEBUG_H

void dputs(const char *str);
void dputchar(char c);

void debug_dump_memory_words(void *mem, int len);

#endif
