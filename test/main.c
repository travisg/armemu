#include "display.h"
#include "text.h"
#include "console.h"
#include "memmap.h"
#include "block.h"

static int has_display = 0;

void arm_enable_ints(void);
void armfunc(int a);
void armasm(void);

// gcc seems to emit a call to this
void __gccmain(void)
{
}

void cpufunc(void)
{
//  thumbfunc(17);
    armasm();
    armfunc(17);
}

int main(void)
{
    int i;
    char c;

    cpufunc();

    if (*REG(SYSINFO_FEATURES) & SYSINFO_FEATURE_DISPLAY)
        has_display = 1;

//  for(i = 0xff; i >= 0; i -= 0x10)
//      clear_display(i | (i<<8) | (i<<16) | (i<<24));

    if (has_display) {
        clear_display(0);
        initialize_text();
        set_text_color(0xffffffff, 0);
    }

    puts("console initialized\n");

    puts("emulator features:\n");
    if (*REG(SYSINFO_FEATURES) & SYSINFO_FEATURE_DISPLAY)
        puts("\tdisplay\n");
    if (*REG(SYSINFO_FEATURES) & SYSINFO_FEATURE_CONSOLE)
        puts("\tconsole\n");
    if (*REG(SYSINFO_FEATURES) & SYSINFO_FEATURE_NETWORK)
        puts("\tnetwork\n");
    if (*REG(SYSINFO_FEATURES) & SYSINFO_FEATURE_BLOCKDEV)
        puts("\tblockdevice\n");

    read_cpu_id();

    mmu_init();

    puts("enabling interrupts\n");
    arm_enable_ints();

    puts("setting timer\n");
    *REG(PIT_INTERVAL) = 500;
    *REG(PIT_START_PERIODIC) = 1;

//  *REG(DEBUG_SET_TRACELEVEL_SYS) = 5;
    *REG(BDEV_CMD_ADDR) = 0x200000;
    *REG64(BDEV_CMD_OFF) = 1024;
    *REG(BDEV_CMD_LEN) = 4096;
    *REG(BDEV_CMD) = BDEV_CMD_ERASE;
    *REG(BDEV_CMD) = BDEV_CMD_WRITE;
    *REG(BDEV_CMD) = BDEV_CMD_READ;

    *REG(BDEV_CMD_ADDR) = 0;
    *REG64(BDEV_CMD_OFF) = 0;
    *REG(BDEV_CMD_LEN) = 4096;
    *REG(BDEV_CMD) = BDEV_CMD_WRITE;
//  *REG(DEBUG_SET_TRACELEVEL_SYS) = 1;

    puts("keyboard test:\n");
    c = 'a';
    unsigned long long off = 0;
    for (;;) {
        unsigned int key;

        /* do some cpu intensive stuff for a bit */
        cpufunc();

        /* see if a keyboard interrupt went off */
        if (read_keyboard(&key) >= 0)
            if ((key & KEY_MOD_UP) == 0)
                putchar(key);

        /* do some block io */
//      block_read(off, 1024*1024, 0x200000);
//      off += 1024*1024;

//      puts("abc ");
//      draw_char('a', 0, 0);
//      draw_char('b', 6, 0);
//      draw_char('c', 12, 0);
//      test_display();
    }
}

void irq_handler(void)
{
    int vector;

    /* test the codepage removal */
    arm_invalidate_i_cache();

#if 0
    *REG(SYSINFO_TIME_LATCH) = 0; // latch the system time
    static unsigned int lasttime; // in us
    unsigned int time;

    unsigned int rawtime[2];
    rawtime[0] = *REG(SYSINFO_TIME_SECS);
    rawtime[1] = *REG(SYSINFO_TIME_USECS);

    time = ((rawtime[0] * 1000000) + rawtime[1]);

    unsigned int delta = time - lasttime;
    debug_dump_memory_words(&delta, 1);

    lasttime = time;
#endif

    vector = *REG(PIC_CURRENT_NUM);
    switch (vector) {
        case INT_PIT:
            dputs("irq timer\n");
            *REG(PIT_CLEAR_INT) = 1;
            break;
        case INT_KEYBOARD:
            dputs("irq keyboard\n");
            keyboard_int_handler();
            break;
        case INT_NET:
            dputs("irq network\n");
            network_int_handler();
            break;
        default:
            puts("unknown irq\n");
            break;
        case 0xffffffff:
            /* false alarm */
            break;
    }
}

void fiq_handler(void)
{
    puts("fiq\n");
}

void data_abort_handler(void)
{
    puts("data abort\n");

    puts("spinning forever...\n");
    for (;;);
}

void prefetch_abort_handler(void)
{
    puts("prefetch abort\n");

    puts("spinning forever...\n");
    for (;;);
}

void undefined_handler(void)
{
    puts("undefined\n");
    puts("spinning forever...\n");
    for (;;);
}

void swi_handler(void)
{
    puts("swi handler\n");
}

