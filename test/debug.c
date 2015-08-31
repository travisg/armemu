#include "debug.h"
#include "memmap.h"

void dputs(const char *str)
{
    while (*str) {
        dputchar(*str);
        str++;
    }
}

void dputchar(char c)
{
    *REG(DEBUG_STDOUT) = c;
}

void debug_dump_memory_bytes(const void *mem, int len)
{
    *REG(DEBUG_MEMDUMPADDR) = (unsigned int)mem;
    *REG(DEBUG_MEMDUMPLEN) = len;
    *REG(DEBUG_MEMDUMP_BYTE) = 1;
}

void debug_dump_memory_words(const void *mem, int len)
{
    *REG(DEBUG_MEMDUMPADDR) = (unsigned int)mem;
    *REG(DEBUG_MEMDUMPLEN) = len;
    *REG(DEBUG_MEMDUMP_WORD) = 1;
}


