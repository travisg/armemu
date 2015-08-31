#include "mmu.h"

/* main translation table */
static unsigned int ttable[16384/4] __attribute__((aligned(16384)));

void mmu_init(void)
{
    unsigned int i;
    unsigned int cp15;

    /* set up the translation table with identity mapped sections */
    for (i=0; i < 4096; i++) {
        ttable[i] = (i<<20) | (3<<10) | (0<<5) | (0<<2) | (2<<0);
    }

    /* point the mmu at the translation table */
    write_trans_table_base(ttable);

    /* set up the domain control register */
    write_domain_control_reg(0x1); // first domain is user, rest is unused

#if 1
    /* enable the mmu */
    puts("enabling mmu\n");
    cp15 = read_cp15();
    cp15 |= 1; // enable mmu
    cp15 |= 2; // enable alignment checking
    cp15 &= ~(3<<8); // clear S & R bit
    write_cp15(cp15);
    puts("mmu enabled\n");
#endif
}
