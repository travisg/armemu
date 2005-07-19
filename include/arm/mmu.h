#ifndef __ARM_MMU_H
#define __ARM_MMU_H

/* memory access routines.
 * if return is true, an exception was thrown and caller should
 * immediately abort the current operation and let the exception
 * handling code for the cpu run.
 */
bool mmu_read_instruction_word(armaddr_t address, word *data, bool priviledged);
bool mmu_read_instruction_halfword(armaddr_t address, halfword *data, bool priviledged);
bool mmu_read_mem_word(armaddr_t address, word *data);
bool mmu_read_mem_halfword(armaddr_t address, halfword *data);
bool mmu_read_mem_byte(armaddr_t address, byte *data);

bool mmu_write_mem_word(armaddr_t address, word data);
bool mmu_write_mem_halfword(armaddr_t address, halfword data);
bool mmu_write_mem_byte(armaddr_t address, byte data);

/* initialization */
void mmu_init(int with_mmu);

/* mmu flags (cr1 flags in cp15) */
#define MMU_ENABLED_FLAG           1
#define MMU_ALIGNMENT_FAULT_FLAG  (1<<1)
#define MMU_SYS_PROTECTION_FLAG   (1<<8)
#define MMU_ROM_PROTECTION_FLAG   (1<<9)
#define MMU_FLAG_MASK (1 | (1<<1) | (1<<8) | (1<<9))
word mmu_set_flags(word flags);
word mmu_get_flags(void);

#define MMU_PAGESIZE 4096
#define MMU_PAGESIZE_SHIFT 12

/* getting and setting internal mmu registers */
enum mmu_registers {
	MMU_TRANS_TABLE_REG,
	MMU_DOMAIN_ACCESS_CONTROL_REG,
	MMU_FAULT_STATUS_REG,
	MMU_FAULT_ADDRESS_REG,
};

void mmu_set_register(enum mmu_registers reg, word val);
word mmu_get_register(enum mmu_registers reg);

#endif
