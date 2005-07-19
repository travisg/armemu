#ifndef __MEMMAP_H
#define __MEMMAP_H

/* some helpful macros */
#define REG(x) ((volatile unsigned int *)(x))
#define REG_H(x) ((volatile unsigned short *)(x))
#define REG_B(x) ((volatile unsigned char *)(x))

/* memory map of our generic arm system */
#define MAINMEM_BASE 0x0
#define MAINMEM_SIZE (4*1024*1024)

/* peripherals are all mapped here */
#define PERIPHERAL_BASE   (0xf0000000)

/* display */
#define DISPLAY_BASE      (PERIPHERAL_BASE)
#define DISPLAY_SIZE      (4*1024*1024)
#define DISPLAY_FRAMEBUFFER DISPLAY_BASE
#define DISPLAY_REGS_BASE (DISPLAY_BASE + DISPLAY_SIZE)
#define DISPLAY_REGS_SIZE (4*1024*1024)
	/* no display regs for now */

/* console (keyboard controller */
#define CONSOLE_REGS_BASE (DISPLAY_REGS_BASE + DISPLAY_REGS_SIZE)
#define CONSOLE_REGS_SIZE (4*1024*1024)
#define KYBD_STAT         (CONSOLE_REGS_BASE + 0)
#define KYBD_DATA         (CONSOLE_REGS_BASE + 4)

/* interrupt controller */
#define PIC_REGS_BASE     (CONSOLE_REGS_BASE + CONSOLE_REGS_SIZE)
#define PIC_REGS_SIZE     (4*1024*1024)

	/* mask any of the 32 interrupt vectors by writing a 1 in the appropriate bit */
#define PIC_MASK          (PIC_REGS_BASE + 0)
	/* each bit corresponds to the current status of the interrupt line */
#define PIC_STAT          (PIC_REGS_BASE + 4)
	/* one bit set for the current interrupt. */
    /* write one to any bit to clear it's status if it's edge triggered. */
#define PIC_CURRENT_BIT   (PIC_REGS_BASE + 8)
	/* holds the current interrupt number, check PIC_CURRENT_BIT to see if something is pending */
#define PIC_CURRENT_NUM   (PIC_REGS_BASE + 12)

	/* interrupt map */
#define INT_KEYBOARD 0
#define PIC_MAX_INT 32

#endif
