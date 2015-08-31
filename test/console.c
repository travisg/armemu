#include "console.h"
#include "memmap.h"

#define KB_BUFSIZE 1024

static unsigned int kb_buf[KB_BUFSIZE];
static volatile int head = 0;
static volatile int tail = 0;

#define IS_BUF_FULL (((tail - 1) % KB_BUFSIZE) == head)
#define IS_BUF_EMPTY (tail == head)
#define USED_BUF_ENTRIES ((head - tail) % KB_BUFSIZE)
#define REMAING_BUF_ENTRIES (KB_BUFSIZE - USED_BUF_ENTRIES)

static int read_keyboard_hardware(unsigned int *key)
{
    if (*REG(KYBD_STAT) == 0)
        return -1; /* nothing available */

    *key = *(REG(KYBD_DATA));
    return 0;
}

void keyboard_int_handler(void)
{
    unsigned int key;

    while (read_keyboard_hardware(&key) >= 0) {
        // we have something
        if (!IS_BUF_FULL) {
            kb_buf[head] = key;
            head = (head + 1) % KB_BUFSIZE;
        }
    }
}

int read_keyboard(unsigned int *key)
{
    if (!IS_BUF_EMPTY) {
        *key = kb_buf[tail];
        tail = (tail + 1) % KB_BUFSIZE;
        return 0;
    }

    return -1;
}


