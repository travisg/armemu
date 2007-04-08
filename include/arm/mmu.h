/*
 * Copyright (c) 2005 Travis Geiselbrecht
 *
 * Permission is hereby granted, free of charge, to any person obtaining
 * a copy of this software and associated documentation files
 * (the "Software"), to deal in the Software without restriction,
 * including without limitation the rights to use, copy, modify, merge,
 * publish, distribute, sublicense, and/or sell copies of the Software,
 * and to permit persons to whom the Software is furnished to do so,
 * subject to the following conditions:
 * 
 * The above copyright notice and this permission notice shall be
 * included in all copies or substantial portions of the Software.
 * 
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
 * IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY
 * CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,
 * TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE
 * SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 */
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
#define MMU_ENABLED_FLAG          (1<<0)
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
void mmu_invalidate_tcache(void);

#endif
