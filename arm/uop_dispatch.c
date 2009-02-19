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
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <debug.h>
#include <options.h>
#include <arm/arm.h>
#include <arm/decoder.h>
#include <util/atomic.h>
#include <util/math.h>

#define ASSERT_VALID_REG(x) ASSERT((x) < 16);

#define DATA_PROCESSING_OP_TABLE(opcode, result, a, b, arith_op, Rd_writeback, carry, ovl) \
	switch(opcode) { \
		case AOP_AND: /* AND */ \
			result = a & b; \
			break; \
		case AOP_EOR: /* EOR */ \
			result = a ^ b; \
			break; \
		case AOP_SUB: /* SUB */ \
			result = do_add(a, ~b, 1, &carry, &ovl); \
			arith_op = 1; \
			break; \
		case AOP_RSB: /* RSB */ \
			result = do_add(b, ~a, 1, &carry, &ovl); \
			arith_op = 1; \
			break; \
		case AOP_ADD: /* ADD */ \
			result = do_add(a, b, 0, &carry, &ovl); \
			arith_op = 1; \
			break; \
		case AOP_ADC: /* ADC */ \
			result = do_add(a, b, get_condition(PSR_CC_CARRY) ? 1 : 0, &carry, &ovl); \
			arith_op = 1; \
			break; \
		case AOP_SBC: /* SBC */ \
			result = do_add(a, ~b, get_condition(PSR_CC_CARRY) ? 1 : 0, &carry, &ovl); \
			arith_op = 1; \
			break; \
		case AOP_RSC: /* RSC */ \
			result = do_add(b, ~a, get_condition(PSR_CC_CARRY) ? 1 : 0, &carry, &ovl); \
			arith_op = 1; \
			break; \
		case AOP_TST: /* TST */ \
			result = a & b; \
			Rd_writeback = 0; \
			break; \
		case AOP_TEQ: /* TEQ */ \
			result = a ^ b; \
			Rd_writeback = 0; \
			break; \
		case AOP_CMP: /* CMP */ \
			result = do_add(a, ~b, 1, &carry, &ovl); \
			Rd_writeback = 0; \
			arith_op = 1; \
			break; \
		case AOP_CMN: /* CMN */ \
			result = do_add(a, b, 0, &carry, &ovl); \
			Rd_writeback = 0; \
			arith_op = 1; \
			break; \
		case AOP_ORR: /* ORR */ \
			result = a | b; \
			break; \
		case AOP_MOV: /* MOV */ \
			result = b; \
			break; \
		case AOP_BIC: /* BIC */ \
			result = a & (~b); \
			break; \
		case AOP_MVN: /* MVN */ \
			result = ~b; \
			break; \
	}

#define DATA_PROCESSING_OP_TABLE_NOFLAGS(opcode, result, a, b) \
	switch(opcode) { \
		case AOP_AND: /* AND */ \
			result = a & b; \
			break; \
		case AOP_EOR: /* EOR */ \
			result = a ^ b; \
			break; \
		case AOP_SUB: /* SUB */ \
			result = a - b; \
			break; \
		case AOP_RSB: /* RSB */ \
			result = b - a; \
			break; \
		case AOP_ADD: /* ADD */ \
			result = a + b; \
			break; \
		case AOP_ADC: /* ADC */ \
			result = a + b + (get_condition(PSR_CC_CARRY) ? 1 : 0); \
			break; \
		case AOP_SBC: /* SBC */ \
			result = a + ~b + (get_condition(PSR_CC_CARRY) ? 1: 0); \
			break; \
		case AOP_RSC: /* RSC */ \
			result = b + ~a + (get_condition(PSR_CC_CARRY) ? 1: 0); \
			break; \
		case AOP_TST: /* TST */ \
			result = a & b; \
			break; \
		case AOP_TEQ: /* TEQ */ \
			result = a ^ b; \
			break; \
		case AOP_CMP: /* CMP */ \
			result = a - b; \
			break; \
		case AOP_CMN: /* CMN */ \
			result = a + b; \
			break; \
		case AOP_ORR: /* ORR */ \
			result = a | b; \
			break; \
		case AOP_MOV: /* MOV */ \
			result = b; \
			break; \
		case AOP_BIC: /* BIC */ \
			result = a & (~b); \
			break; \
		case AOP_MVN: /* MVN */ \
			result = ~b; \
			break; \
	}

void uop_init(void)
{
	memset(cpu.codepage_hash, 0, sizeof(cpu.codepage_hash));
	cpu.curr_cp = NULL;
}

const char *uop_opcode_to_str(int opcode)
{
#define OP_TO_STR(op) case op: return #op

	switch(opcode) {
		OP_TO_STR(NOP);
		OP_TO_STR(DECODE_ME_ARM);
		OP_TO_STR(DECODE_ME_THUMB);
		OP_TO_STR(B_IMMEDIATE);
		OP_TO_STR(B_IMMEDIATE_LOCAL);
		OP_TO_STR(B_REG);
		OP_TO_STR(B_REG_OFFSET);
		OP_TO_STR(LOAD_IMMEDIATE_WORD);
		OP_TO_STR(LOAD_IMMEDIATE_HALFWORD);
		OP_TO_STR(LOAD_IMMEDIATE_BYTE);
		OP_TO_STR(LOAD_IMMEDIATE_OFFSET);
		OP_TO_STR(LOAD_SCALED_REG_OFFSET);
		OP_TO_STR(STORE_IMMEDIATE_OFFSET);
		OP_TO_STR(STORE_SCALED_REG_OFFSET);
		OP_TO_STR(LOAD_MULTIPLE);
		OP_TO_STR(LOAD_MULTIPLE_S);
		OP_TO_STR(STORE_MULTIPLE);
		OP_TO_STR(STORE_MULTIPLE_S);
		OP_TO_STR(DATA_PROCESSING_IMM);
		OP_TO_STR(DATA_PROCESSING_IMM_S);
		OP_TO_STR(DATA_PROCESSING_REG);
		OP_TO_STR(DATA_PROCESSING_REG_S);
		OP_TO_STR(DATA_PROCESSING_IMM_SHIFT);
		OP_TO_STR(DATA_PROCESSING_REG_SHIFT);
		OP_TO_STR(MOV_IMM);
		OP_TO_STR(MOV_IMM_NZ);
		OP_TO_STR(MOV_REG);
		OP_TO_STR(CMP_IMM_S);
		OP_TO_STR(CMP_REG_S);
		OP_TO_STR(CMN_REG_S);
		OP_TO_STR(TST_REG_S);
		OP_TO_STR(ADD_IMM);
		OP_TO_STR(ADD_IMM_S);
		OP_TO_STR(ADD_REG);
		OP_TO_STR(ADD_REG_S);
		OP_TO_STR(ADC_REG_S);
		OP_TO_STR(SUB_REG_S);
		OP_TO_STR(SBC_REG_S);
		OP_TO_STR(ORR_IMM);
		OP_TO_STR(ORR_REG_S);
		OP_TO_STR(LSL_IMM);
		OP_TO_STR(LSL_IMM_S);
		OP_TO_STR(LSL_REG);
		OP_TO_STR(LSL_REG_S);
		OP_TO_STR(LSR_IMM);
		OP_TO_STR(LSR_IMM_S);
		OP_TO_STR(LSR_REG);
		OP_TO_STR(LSR_REG_S);
		OP_TO_STR(ASR_IMM);
		OP_TO_STR(ASR_IMM_S);
		OP_TO_STR(ASR_REG);
		OP_TO_STR(ASR_REG_S);
		OP_TO_STR(ROR_REG);
		OP_TO_STR(ROR_REG_S);
		OP_TO_STR(AND_REG_S);
		OP_TO_STR(EOR_REG_S);
		OP_TO_STR(BIC_REG_S);
		OP_TO_STR(NEG_REG_S);
		OP_TO_STR(MVN_REG_S);
		OP_TO_STR(MULTIPLY);
		OP_TO_STR(MULTIPLY_LONG);
		OP_TO_STR(COUNT_LEADING_ZEROS);
		OP_TO_STR(MOVE_TO_SR_IMM);
		OP_TO_STR(MOVE_TO_SR_REG);
		OP_TO_STR(MOVE_FROM_SR);
		OP_TO_STR(UNDEFINED);
		OP_TO_STR(SWI);
		OP_TO_STR(BKPT);
		OP_TO_STR(COPROC_REG_TRANSFER);
		OP_TO_STR(COPROC_DOUBLE_REG_TRANSFER);
		OP_TO_STR(COPROC_DATA_PROCESSING);
		OP_TO_STR(COPROC_LOAD_STORE);
		default: return "UNKNOWN";
	}
#undef OP_TO_STR
}

/* codepage cache */
static void free_codepage(struct uop_codepage *cp)
{
	UOP_TRACE(7, "free_codepage: cp %p, thumb %d, address 0x%x\n", cp, cp->thumb, cp->address);
	if (cp->thumb) {
		cp->next = cpu.free_cp_thumb;
		cpu.free_cp_thumb = cp;
	} else {
		cp->next = cpu.free_cp_arm;
		cpu.free_cp_arm = cp;
	}
}

#define CP_PREALLOCATE 16
#define ARM_CP_SIZE (sizeof(struct uop_codepage) + sizeof(struct uop) * (NUM_CODEPAGE_INS_ARM + 1))
#define THUMB_CP_SIZE (sizeof(struct uop_codepage) + sizeof(struct uop) * (NUM_CODEPAGE_INS_THUMB + 1))

static struct uop_codepage *alloc_codepage_arm(void)
{
	struct uop_codepage *cp;
	
	if (cpu.free_cp_arm == NULL) {
		int i;

		uint8_t *cp_buf = malloc(ARM_CP_SIZE * CP_PREALLOCATE);
		if (cp_buf == NULL)
			return NULL;

		for (i=0; i < CP_PREALLOCATE; i++) {
			cp = (struct uop_codepage *)(cp_buf + (i * ARM_CP_SIZE));
			cp->thumb = FALSE;
			cp->pc_inc = 4;
			cp->pc_shift = 2;
			free_codepage(cp);			
		}
	} 

	ASSERT(cpu.free_cp_arm != NULL);
	cp = cpu.free_cp_arm;
	cpu.free_cp_arm = cp->next;
	cp->next = NULL;

	ASSERT(cp->thumb == FALSE);

	return cp;
}

static struct uop_codepage *alloc_codepage_thumb(void)
{
	struct uop_codepage *cp;

	if (cpu.free_cp_thumb == NULL) {
		int i;

		uint8_t *cp_buf = malloc(THUMB_CP_SIZE * CP_PREALLOCATE);
		if (cp_buf == NULL)
			return NULL;

		for (i=0; i < CP_PREALLOCATE; i++) {
			cp = (struct uop_codepage *)(cp_buf + (i * THUMB_CP_SIZE));
			cp->thumb = TRUE;
			cp->pc_inc = 2;
			cp->pc_shift = 1;
			free_codepage(cp);			
		}
	} 

	ASSERT(cpu.free_cp_thumb != NULL);
	cp = cpu.free_cp_thumb;
	cpu.free_cp_thumb = cp->next;
	cp->next = NULL;

	ASSERT(cp->thumb == TRUE);
	return cp;
}

#define PC_TO_CPPC(pc) &cpu.curr_cp->ops[((pc) % MMU_PAGESIZE) >> (cpu.curr_cp->pc_shift)];

static inline unsigned int codepage_hash(armaddr_t address, bool thumb)
{
	unsigned int hash = (address / MMU_PAGESIZE);
	if(thumb)
		hash = ~hash; // XXX come up with something better
	return hash % CODEPAGE_HASHSIZE;
}

static struct uop_codepage *lookup_codepage(armaddr_t pc, bool thumb)
{
	armaddr_t cp_addr = pc & ~(MMU_PAGESIZE-1);
	unsigned int hash;
	struct uop_codepage *cp;

	hash = codepage_hash(cp_addr, thumb);

	// search the hash chain for our codepage
	cp = cpu.codepage_hash[hash];
	while(cp != NULL) {
		if(cp->address == cp_addr && cp->thumb == thumb)
			return cp;
		cp = cp->next;
	}

	return NULL;
}

static bool load_codepage_arm(armaddr_t pc, armaddr_t cp_addr, bool priviledged, struct uop_codepage **_cp, int *last_ins_index)
{
	struct uop_codepage *cp;	
	int i;

	cp = alloc_codepage_arm();
	if(!cp)
		panic_cpu("could not allocate new codepage!\n");

	// load and fill in the default codepage
	cp->address = cp_addr;
	for(i=0; i < NUM_CODEPAGE_INS_ARM; i++) {
		cp->ops[i].opcode = DECODE_ME_ARM;
		cp->ops[i].cond = COND_AL;
		cp->ops[i].flags = 0;
		if(mmu_read_instruction_word(cp_addr + i*4, &cp->ops[i].undecoded.raw_instruction, priviledged)) {
			UOP_TRACE(4, "load_codepage: mmu translation made arm codepage load fail\n");
			free(cp);
			return TRUE;
		}
	}
	*last_ins_index = NUM_CODEPAGE_INS_ARM;

	*_cp = cp;

	return FALSE;
}

static bool load_codepage_thumb(armaddr_t pc, armaddr_t cp_addr, bool priviledged, struct uop_codepage **_cp, int *last_ins_index)
{
	struct uop_codepage *cp;	
	int i;

	cp = alloc_codepage_thumb();
	if(!cp)
		panic_cpu("could not allocate new codepage!\n");

	// load and fill in the default codepage
	cp->address = cp_addr;
	for(i=0; i < NUM_CODEPAGE_INS_THUMB; i++) {
		halfword hword;
		
		cp->ops[i].opcode = DECODE_ME_THUMB;
		cp->ops[i].cond = COND_AL;
		cp->ops[i].flags = 0;
		if(mmu_read_instruction_halfword(cp_addr + i*2, &hword, priviledged)) {
			UOP_TRACE(4, "load_codepage: mmu translation made thumb codepage load fail\n");
			free(cp);
			return TRUE;
		}
		cp->ops[i].undecoded.raw_instruction = hword;
	}
	*last_ins_index = NUM_CODEPAGE_INS_THUMB;

	*_cp = cp;

	return FALSE;
}

static bool load_codepage(armaddr_t pc, bool thumb, bool priviledged, struct uop_codepage **_cp)
{
	armaddr_t cp_addr = pc & ~(MMU_PAGESIZE-1);
	struct uop_codepage *cp;	
	unsigned int hash;
	int last_ins_index;
	bool ret;

	UOP_TRACE(4, "load_codepage: pc 0x%x\n", pc);

	// load and fill in the appropriate codepage
	if(thumb)
		ret = load_codepage_thumb(pc, cp_addr, priviledged, &cp, &last_ins_index);
	else
		ret = load_codepage_arm(pc, cp_addr, priviledged, &cp, &last_ins_index);
	if(ret)
		return TRUE; // there was some sort of error loading the new codepage

	// fill in the last instruction to branch to next codepage
	cp->ops[last_ins_index].opcode = B_IMMEDIATE;
	cp->ops[last_ins_index].cond = COND_AL;
	cp->ops[last_ins_index].flags = 0;
	cp->ops[last_ins_index].b_immediate.target = cp->address + MMU_PAGESIZE;
	cp->ops[last_ins_index].b_immediate.link_target = 0;
	cp->ops[last_ins_index].b_immediate.target_cp = NULL;

	// add it to the codepage hashtable
	hash = codepage_hash(cp_addr, thumb);
	cp->next = cpu.codepage_hash[hash];
	cpu.codepage_hash[hash] = cp;

	*_cp = cp;

	return FALSE;
}

static bool set_codepage(armaddr_t pc)
{
	struct uop_codepage *cp;
	bool thumb = get_condition(PSR_THUMB) ? TRUE : FALSE;

	UOP_TRACE(6, "set_codepage: pc 0x%x thumb %d, real pc 0x%x\n", pc, thumb, cpu.pc);

	cp = lookup_codepage(cpu.pc, thumb);
	if(unlikely(cp == NULL)) { // still null, create a new one
		UOP_TRACE(7, "set_codepage: couldn't find codepage, creating new one\n");
		if(load_codepage(pc, thumb, arm_in_priviledged(), &cpu.curr_cp)) {
			return TRUE; // mmu fault
		}
	} else {
		UOP_TRACE(7, "set_codepage: found cached codepage\n");
		cpu.curr_cp = cp;
	}

	ASSERT(cpu.curr_cp != NULL);
	cpu.cp_pc = PC_TO_CPPC(cpu.pc);
	return FALSE;
}

void flush_all_codepages(void)
{
	int i;
	struct uop_codepage *cp;

	/* XXX make this smarter */
	for (i=0; i < CODEPAGE_HASHSIZE; i++) {
		cp = cpu.codepage_hash[i];

		while (cp != NULL) {
			struct uop_codepage *temp = cp;
			cp = cp->next;
			free_codepage(temp);
		}
		cpu.codepage_hash[i] = NULL;
	}

	/* force a reload of the current codepage */
	cpu.curr_cp = NULL;
}

static inline __ALWAYS_INLINE void uop_decode_me_arm(struct uop *op) 
{
	// call the arm decoder and set the pc back to retry this instruction
	ASSERT(cpu.cp_pc != NULL);
	UOP_TRACE(6, "decoding arm opcode 0x%08x at pc 0x%x\n", op->undecoded.raw_instruction, cpu.pc);
	arm_decode_into_uop(op);
	cpu.pc -= 4; // back the instruction pointer up to retry this instruction
	cpu.cp_pc--;
	inc_perf_counter(INS_DECODE);
}

static inline __ALWAYS_INLINE void uop_decode_me_thumb(struct uop *op) 
{
	// call the arm decoder and set the pc back to retry this instruction
	ASSERT(cpu.cp_pc != NULL);
	UOP_TRACE(6, "decoding thumb opcode 0x%04x at pc 0x%x\n", op->undecoded.raw_instruction, cpu.pc);
	thumb_decode_into_uop(op);
	cpu.pc -= 2; // back the instruction pointer up to retry this instruction
	cpu.cp_pc--;
	inc_perf_counter(INS_DECODE);
}

static inline __ALWAYS_INLINE void uop_b_immediate(struct uop *op)
{
	// branch to a fixed location outside of the current codepage. 
	// Any offsets would have been resolved at decode time.
	if(op->flags & UOPBFLAGS_LINK) {
		int thumb = get_condition(PSR_THUMB) ? 1 : 0;
		put_reg(LR, op->b_immediate.link_target | thumb);
	}

	if(op->flags & UOPBFLAGS_SETTHUMB_ALWAYS) {
		set_condition(PSR_THUMB, TRUE);
		// force a codepage reload
		cpu.curr_cp = NULL;
	}
	if(op->flags & UOPBFLAGS_UNSETTHUMB_ALWAYS) {
		set_condition(PSR_THUMB, FALSE);
		// force a codepage reload
		cpu.curr_cp = NULL;
	}

	cpu.pc = op->b_immediate.target;
	if(likely(op->b_immediate.target_cp != NULL)) {
		// we have already cached a pointer to the target codepage, use it
		cpu.curr_cp = op->b_immediate.target_cp;
		cpu.cp_pc = PC_TO_CPPC(cpu.pc);
	} else {
		// see if we can lookup the target codepage and try again
		struct uop_codepage *cp = lookup_codepage(cpu.pc, get_condition(PSR_THUMB) ? TRUE : FALSE);
		if(cp != NULL) {
			// found one, cache it and set the code page
			op->b_immediate.target_cp = cp;
			cpu.curr_cp = cp;
			cpu.cp_pc = PC_TO_CPPC(cpu.pc);
		} else {
			// didn't find one, force a codepage reload next instruction
			cpu.curr_cp = NULL;
		}
	}

#if COUNT_ARM_OPS
	inc_perf_counter(OP_BRANCH);
#endif
#if COUNT_CYCLES
	// all branch instructions take 3 cycles on all cores
	add_to_perf_counter(CYCLE_COUNT, 2);
#endif
}

static inline __ALWAYS_INLINE void uop_b_immediate_local(struct uop *op)
{
	// branch to a fixed location within the current codepage.
	if(op->flags & UOPBFLAGS_LINK) {
		int thumb = get_condition(PSR_THUMB) ? 1 : 0;
		put_reg(LR, op->b_immediate.link_target | thumb);
	}

	cpu.pc = op->b_immediate.target;
	ASSERT(cpu.curr_cp != NULL);
	cpu.cp_pc = PC_TO_CPPC(cpu.pc);
#if COUNT_ARM_OPS
	inc_perf_counter(OP_BRANCH);
#endif
#if COUNT_CYCLES
	// all branch instructions take 3 cycles on all cores
	add_to_perf_counter(CYCLE_COUNT, 2);
#endif
}

static inline __ALWAYS_INLINE void uop_b_reg(struct uop *op)
{
	armaddr_t temp_addr;

	temp_addr = get_reg(op->b_reg.reg);

	// branch to register contents
	if(op->flags & UOPBFLAGS_LINK) {
		int thumb = get_condition(PSR_THUMB) ? 1 : 0;
		put_reg(LR, (get_reg(PC) + op->b_reg.link_offset) | thumb);
	}

	put_reg(PC, temp_addr & 0xfffffffe);

	if((temp_addr >> MMU_PAGESIZE_SHIFT) == (cpu.pc >> MMU_PAGESIZE_SHIFT)) {
		// it's a local branch, just recalc the position in the current codepage
		cpu.pc = temp_addr & 0xfffffffe;
		cpu.cp_pc = PC_TO_CPPC(cpu.pc);
	} else {
		// it's a remote branch
		cpu.pc = temp_addr & 0xfffffffe;
		cpu.curr_cp = NULL;
	}

	if(op->flags & UOPBFLAGS_SETTHUMB_ALWAYS) {
		set_condition(PSR_THUMB, TRUE);
		// force a codepage reload
		cpu.curr_cp = NULL;
	}
	if(op->flags & UOPBFLAGS_UNSETTHUMB_ALWAYS) {
		set_condition(PSR_THUMB, FALSE);
		// force a codepage reload
		cpu.curr_cp = NULL;
	}

	// if the bottom bit of the target address is 1, switch to thumb, otherwise switch to arm
	if(op->flags & UOPBFLAGS_SETTHUMB_COND) {
		bool old_condition = get_condition(PSR_THUMB) ? TRUE : FALSE;
		bool new_condition = (temp_addr & 1) ? TRUE : FALSE;

		if(old_condition != new_condition) {
			set_condition(PSR_THUMB, new_condition);
			// force a codepage reload
			cpu.curr_cp = NULL;
			UOP_TRACE(7, "B_REG: setting thumb to %d (new mode)\n", new_condition);
		}
	}

#if COUNT_ARM_OPS
	inc_perf_counter(OP_BRANCH);
#endif
#if COUNT_CYCLES
	// all branch instructions take 3 cycles on all cores
	add_to_perf_counter(CYCLE_COUNT, 2);
#endif
}

static inline __ALWAYS_INLINE void uop_b_reg_offset(struct uop *op)
{
	armaddr_t temp_addr;

	temp_addr = get_reg(op->b_reg_offset.reg);
	temp_addr += op->b_reg_offset.offset;

	// branch to register contents
	if(op->flags & UOPBFLAGS_LINK) {
		int thumb = get_condition(PSR_THUMB) ? 1 : 0;
		put_reg(LR, (get_reg(PC) + op->b_reg_offset.link_offset) | thumb);
	}

	put_reg(PC, temp_addr & 0xfffffffe);

	if((temp_addr >> MMU_PAGESIZE_SHIFT) == (cpu.pc >> MMU_PAGESIZE_SHIFT)) {
		// it's a local branch, just recalc the position in the current codepage
		cpu.pc = temp_addr & 0xfffffffe;
		cpu.cp_pc = PC_TO_CPPC(cpu.pc);
	} else {
		// it's a remote branch
		cpu.pc = temp_addr & 0xfffffffe;
		cpu.curr_cp = NULL;
	}
	
	if(op->flags & UOPBFLAGS_SETTHUMB_ALWAYS) {
		set_condition(PSR_THUMB, TRUE);
		// force a codepage reload
		cpu.curr_cp = NULL;
	}
	if(op->flags & UOPBFLAGS_UNSETTHUMB_ALWAYS) {
		set_condition(PSR_THUMB, FALSE);
		// force a codepage reload
		cpu.curr_cp = NULL;
	}
	
	// if the bottom bit of the target address is 1, switch to thumb, otherwise switch to arm
	if(op->flags & UOPBFLAGS_SETTHUMB_COND) {
		bool old_condition = get_condition(PSR_THUMB) ? TRUE : FALSE;
		bool new_condition = (temp_addr & 1) ? TRUE : FALSE;

		if(old_condition != new_condition) {
			set_condition(PSR_THUMB, new_condition);
			// force a codepage reload
			cpu.curr_cp = NULL;
			UOP_TRACE(7, "B_REG: setting thumb to %d (new mode)\n", new_condition);
		}
	}

#if COUNT_ARM_OPS
	inc_perf_counter(OP_BRANCH);
#endif
#if COUNT_CYCLES
	// all branch instructions take 3 cycles on all cores
	add_to_perf_counter(CYCLE_COUNT, 2);
#endif
}

static inline __ALWAYS_INLINE void uop_load_immediate_word(struct uop *op)
{
	word temp_word;

	// a very simple load, the address is already precalculated
	if(mmu_read_mem_word(op->load_immediate.address, &temp_word))
		return;

	// XXX on armv5 this can switch to thumb
	put_reg(op->load_immediate.target_reg, temp_word);

#if COUNT_ARM_OPS
	inc_perf_counter(OP_LOAD);
#endif
#if COUNT_CYCLES
	// cycle count
	if(op->load_immediate.target_reg == PC)
		add_to_perf_counter(CYCLE_COUNT, 4); // on all cores pc loads are 4 cycles
	else if(get_core() == ARM7)
		add_to_perf_counter(CYCLE_COUNT, 2); // on arm7 all other loads are 3
#endif
}

static inline __ALWAYS_INLINE void uop_load_immediate_halfword(struct uop *op)
{
	halfword temp_halfword;
	word temp_word;

	// a very simple load, the address is already precalculated
	if(mmu_read_mem_halfword(op->load_immediate.address, &temp_halfword))
		return;
	temp_word = temp_halfword;
	if(op->flags & UOPLSFLAGS_SIGN_EXTEND)
		temp_word = SIGN_EXTEND(temp_word, 15);

	// XXX on armv5 this can switch to thumb
	put_reg(op->load_immediate.target_reg, temp_word);

#if COUNT_ARM_OPS
	inc_perf_counter(OP_LOAD);
#endif
#if COUNT_CYCLES
	// cycle count
	if(op->load_immediate.target_reg == PC)
		add_to_perf_counter(CYCLE_COUNT, 4); // on all cores pc loads are 4 cycles
	else if(get_core() == ARM7)
		add_to_perf_counter(CYCLE_COUNT, 2); // on arm7 all other loads are 3
	else if(get_core() >= ARM9)
		add_to_perf_counter(CYCLE_COUNT, 1); // byte and halfword loads are one cycle slower
#endif
}

static inline __ALWAYS_INLINE void uop_load_immediate_byte(struct uop *op)
{
	byte temp_byte;
	word temp_word;

	// a very simple load, the address is already precalculated
	if(mmu_read_mem_byte(op->load_immediate.address, &temp_byte))
		return;
	temp_word = temp_byte;
	if(op->flags & UOPLSFLAGS_SIGN_EXTEND)
		temp_word = SIGN_EXTEND(temp_word, 7);

	// XXX on armv5 this can switch to thumb
	put_reg(op->load_immediate.target_reg, temp_word);

#if COUNT_ARM_OPS
	inc_perf_counter(OP_LOAD);
#endif
#if COUNT_CYCLES
	// cycle count
	if(op->load_immediate.target_reg == PC)
		add_to_perf_counter(CYCLE_COUNT, 4); // on all cores pc loads are 4 cycles
	else if(get_core() == ARM7)
		add_to_perf_counter(CYCLE_COUNT, 2); // on arm7 all other loads are 3
	else if(get_core() >= ARM9)
		add_to_perf_counter(CYCLE_COUNT, 1); // byte and halfword loads are one cycle slower
#endif
}

static inline __ALWAYS_INLINE void uop_load_immediate_offset(struct uop *op)
{
	armaddr_t temp_addr, temp_addr2, temp_addr3;
	word temp_word;

	// slightly more complex, add an offset to a register
	temp_addr2 = get_reg(op->load_store_immediate_offset.source_reg);
	temp_addr3 = temp_addr2 + op->load_store_immediate_offset.offset;

	if(op->flags & UOPLSFLAGS_POSTINDEX)
		temp_addr = temp_addr2; // use the pre-offset computed address
	else
		temp_addr = temp_addr3;

	// read in the difference sizes & sign extend
	switch(op->flags & UOPLSFLAGS_SIZE_MASK) {
		case UOPLSFLAGS_SIZE_WORD:
			if(mmu_read_mem_word(temp_addr, &temp_word))
				return;
			break;
		case UOPLSFLAGS_SIZE_HALFWORD: {
			halfword temp_halfword;

			if(mmu_read_mem_halfword(temp_addr, &temp_halfword))
				return;
			temp_word = temp_halfword;
			if(op->flags & UOPLSFLAGS_SIGN_EXTEND)
				temp_word = SIGN_EXTEND(temp_word, 15);
			break;
		}
		case UOPLSFLAGS_SIZE_BYTE: {
			byte temp_byte;

			if(mmu_read_mem_byte(temp_addr, &temp_byte))
				return;
			temp_word = temp_byte;
			if(op->flags & UOPLSFLAGS_SIGN_EXTEND)
				temp_word = SIGN_EXTEND(temp_word, 7);
			break;
		}
	}

	// store the result
	// XXX on armv5 this can switch to thumb
	put_reg(op->load_store_immediate_offset.target_reg, temp_word);

	// do writeback
	if(op->flags & UOPLSFLAGS_WRITEBACK)
		put_reg(op->load_store_immediate_offset.source_reg, temp_addr3);
#if COUNT_ARM_OPS
	inc_perf_counter(OP_LOAD);
#endif
#if COUNT_CYCLES
	// cycle count
	if(op->load_store_immediate_offset.target_reg == PC)
		add_to_perf_counter(CYCLE_COUNT, 4); // on all cores pc loads are 4 cycles
	else if(get_core() == ARM7)
		add_to_perf_counter(CYCLE_COUNT, 2); // on arm7 all other loads are 3
	else if(get_core() >= ARM9 && (op->flags & UOPLSFLAGS_SIZE_MASK) != UOPLSFLAGS_SIZE_WORD)
		add_to_perf_counter(CYCLE_COUNT, 1); // byte and halfword loads are one cycle slower
#endif
}

static inline __ALWAYS_INLINE void uop_load_scaled_reg_offset(struct uop *op)
{
	armaddr_t temp_addr, temp_addr2, temp_addr3;
	word temp_word;

	// pretty complex. take two registers, optionally perform a shift operation on the second one,
	// add them together and then load that address
	temp_addr2 = get_reg(op->load_store_scaled_reg_offset.source_reg);
	temp_addr3 = get_reg(op->load_store_scaled_reg_offset.source2_reg);	
	switch(op->load_store_scaled_reg_offset.shift_op) {
		case 0: // LSL
			temp_addr3 = LSL(temp_addr3, op->load_store_scaled_reg_offset.shift_immediate);
			break;
		case 1: // LSR
			if(op->load_store_scaled_reg_offset.shift_immediate == 0)
				temp_addr3 = 0;
			else
				temp_addr3 = LSR(temp_addr3, op->load_store_scaled_reg_offset.shift_immediate);
			break;
		case 2: // ASR
			if(op->load_store_scaled_reg_offset.shift_immediate == 0) {
				if(temp_addr3 & 0x80000000)
					temp_addr3 = 0xffffffff;
				else
					temp_addr3 = 0;
			} else {
				temp_addr3 = ASR(temp_addr3, op->load_store_scaled_reg_offset.shift_immediate);
			}
			break;
		case 3: // ROR or RRX
			if(op->load_store_scaled_reg_offset.shift_immediate == 0) { // RRX 
				temp_addr3 = (cpu.cpsr ? 0x80000000 : 0) | LSR(temp_word, 1);
			} else {
				temp_addr3 = ROR(temp_addr3, op->load_store_scaled_reg_offset.shift_immediate);
			}
			break;
	}

	if(op->flags & UOPLSFLAGS_NEGATE_OFFSET)
		temp_addr3 = -temp_addr3;

	temp_addr3 += temp_addr2; // fully calculated address

	if(op->flags & UOPLSFLAGS_POSTINDEX)
		temp_addr = temp_addr2; // use the pre-offset computed address
	else
		temp_addr = temp_addr3;

	// now we have an address, do the load
	// read in the difference sizes & sign extend
	switch(op->flags & UOPLSFLAGS_SIZE_MASK) {
		case UOPLSFLAGS_SIZE_WORD:
			if(mmu_read_mem_word(temp_addr, &temp_word))
				return;
			break;
		case UOPLSFLAGS_SIZE_HALFWORD: {
			halfword temp_halfword;
		
			if(mmu_read_mem_halfword(temp_addr, &temp_halfword))
				return;
			temp_word = temp_halfword;
			if(op->flags & UOPLSFLAGS_SIGN_EXTEND)
				temp_word = SIGN_EXTEND(temp_word, 15);
			break;
		}
		case UOPLSFLAGS_SIZE_BYTE: {
			byte temp_byte;

			if(mmu_read_mem_byte(temp_addr, &temp_byte))
				return;
			temp_word = temp_byte;
			if(op->flags & UOPLSFLAGS_SIGN_EXTEND)
				temp_word = SIGN_EXTEND(temp_word, 7);
			break;
		}
	}

	// store the result
	// XXX on armv5 this can switch to thumb
	put_reg(op->load_store_scaled_reg_offset.target_reg, temp_word);

	// do writeback
	if(op->flags & UOPLSFLAGS_WRITEBACK)
		put_reg(op->load_store_scaled_reg_offset.source_reg, temp_addr);

#if COUNT_ARM_OPS
	inc_perf_counter(OP_LOAD);
#endif
#if COUNT_CYCLES
	// cycle count
	if(op->load_store_scaled_reg_offset.target_reg == PC)
		add_to_perf_counter(CYCLE_COUNT, 4); // on all cores pc loads are 4 cycles
	else if(get_core() == ARM7)
		add_to_perf_counter(CYCLE_COUNT, 2); // on arm7 all other loads are 3
	else if(get_core() >= ARM9 && (op->flags & UOPLSFLAGS_SIZE_MASK) != UOPLSFLAGS_SIZE_WORD)
		add_to_perf_counter(CYCLE_COUNT, 1); // byte and halfword loads are one cycle slower
	if(get_core() == ARM9e)
		add_to_perf_counter(CYCLE_COUNT, 1); // scaled register loads are 1 cycle slower on this core
#endif
}

static inline __ALWAYS_INLINE void uop_store_immediate_offset(struct uop *op)
{
	armaddr_t temp_addr, temp_addr2, temp_addr3;
	word temp_word;

	// slightly more complex, add an offset to a register
	temp_addr2 = get_reg(op->load_store_immediate_offset.source_reg);
	temp_addr3 = temp_addr2 + op->load_store_immediate_offset.offset;

	if(op->flags & UOPLSFLAGS_POSTINDEX)
		temp_addr = temp_addr2; // use the pre-offset computed address
	else
		temp_addr = temp_addr3;

	// read in what we're going to store
	temp_word = get_reg(op->load_store_immediate_offset.target_reg);

	// write it out based on the size we were requested
	switch(op->flags & UOPLSFLAGS_SIZE_MASK) {
		case UOPLSFLAGS_SIZE_WORD:
			if(mmu_write_mem_word(temp_addr, temp_word))
				return;
			break;
		case UOPLSFLAGS_SIZE_HALFWORD:
			if(mmu_write_mem_halfword(temp_addr, temp_word))
				return;
			break;
		case UOPLSFLAGS_SIZE_BYTE:
			if(mmu_write_mem_byte(temp_addr, temp_word))
				return;
			break;
	}

	// do writeback
	if(op->flags & UOPLSFLAGS_WRITEBACK)
		put_reg(op->load_store_immediate_offset.source_reg, temp_addr3);

#if COUNT_ARM_OPS
	inc_perf_counter(OP_STORE);
#endif
#if COUNT_CYCLES
	// cycle count (arm9 is 1 cycle, arm7 is 2)
	if(get_core() == ARM7) {
		add_to_perf_counter(CYCLE_COUNT, 1);
	}
#endif
}


static inline __ALWAYS_INLINE void uop_store_scaled_reg_offset(struct uop *op)
{
	armaddr_t temp_addr, temp_addr2, temp_addr3;
	word temp_word;

	// pretty complex. take two registers, optionally perform a shift operation on the second one,
	// add them together and then load that address
	// XXX room for improvement here, since lots of times I'm sure an instruction
	// decoded to a immediate shift of zero to get plain register add
	temp_addr2 = get_reg(op->load_store_scaled_reg_offset.source_reg);
	temp_addr3 = get_reg(op->load_store_scaled_reg_offset.source2_reg);	
	switch(op->load_store_scaled_reg_offset.shift_op) {
		case 0: // LSL
			temp_addr3 = LSL(temp_addr3, op->load_store_scaled_reg_offset.shift_immediate);
			break;
		case 1: // LSR
			if(op->load_store_scaled_reg_offset.shift_immediate == 0)
				temp_addr3 = 0;
			else
				temp_addr3 = LSR(temp_addr3, op->load_store_scaled_reg_offset.shift_immediate);
			break;
		case 2: // ASR
			if(op->load_store_scaled_reg_offset.shift_immediate == 0) {
				if(temp_addr3 & 0x80000000)
					temp_addr3 = 0xffffffff;
				else
					temp_addr3 = 0;
			} else {
				temp_addr3 = ASR(temp_addr3, op->load_store_scaled_reg_offset.shift_immediate);
			}
			break;
		case 3: // ROR or RRX
			if(op->load_store_scaled_reg_offset.shift_immediate == 0) { // RRX 
				temp_addr3 = (cpu.cpsr ? 0x80000000 : 0) | LSR(temp_addr3, 1);
			} else {
				temp_addr3 = ROR(temp_addr3, op->load_store_scaled_reg_offset.shift_immediate);
			}
			break;
	}

	if(op->flags & UOPLSFLAGS_NEGATE_OFFSET)
		temp_addr3 = -temp_addr3;

	temp_addr3 += temp_addr2; // fully calculated address

	if(op->flags & UOPLSFLAGS_POSTINDEX)
		temp_addr = temp_addr2; // use the pre-offset computed address
	else
		temp_addr = temp_addr3;

	// read in what we're going to store
	temp_word = get_reg(op->load_store_scaled_reg_offset.target_reg);

	// write it out based on the size we were requested
	switch(op->flags & UOPLSFLAGS_SIZE_MASK) {
		case UOPLSFLAGS_SIZE_WORD:
			if(mmu_write_mem_word(temp_addr, temp_word))
				return;
			break;
		case UOPLSFLAGS_SIZE_HALFWORD:
			if(mmu_write_mem_halfword(temp_addr, temp_word))
				return;
			break;
		case UOPLSFLAGS_SIZE_BYTE:
			if(mmu_write_mem_byte(temp_addr, temp_word))
				return;
			break;
	}

	// do writeback
	if(op->flags & UOPLSFLAGS_WRITEBACK)
		put_reg(op->load_store_scaled_reg_offset.source_reg, temp_addr);

#if COUNT_ARM_OPS
	inc_perf_counter(OP_STORE);
#endif
#if COUNT_CYCLES
	// cycle count (arm9 is 1 cycle, arm7 is 2)
	if(get_core() == ARM7) {
		add_to_perf_counter(CYCLE_COUNT, 1);
	} else if(get_core() == ARM9e) {
		add_to_perf_counter(CYCLE_COUNT, 1);
	}
#endif
}

static inline __ALWAYS_INLINE void uop_load_multiple(struct uop *op)
{
	armaddr_t temp_addr, temp_addr2;
	word temp_word;
	int i;
	word reg_list = op->load_store_multiple.reg_bitmap;

	// calculate the base address
	temp_addr = get_reg(op->load_store_multiple.base_reg);
	temp_addr2 = temp_addr + op->load_store_multiple.base_offset;

	// scan through the list of registers, reading in each one
	ASSERT((reg_list >> 16) == 0);
	for(i = 0; reg_list != 0; i++, reg_list >>= 1) {
		if(reg_list & 1) {
			if(mmu_read_mem_word(temp_addr2, &temp_word)) {
				// there was a data abort, and we may have trashed the base register. Restore it.
				put_reg(op->load_store_multiple.base_reg, temp_addr);
				return;
			}

			// XXX on armv5 this can switch to thumb
			put_reg(i, temp_word);
			temp_addr2 += 4;
		}
	}

	// writeback
	if(op->flags & UOPLSMFLAGS_WRITEBACK) {
		temp_addr2 = temp_addr + op->load_store_multiple.writeback_offset;
		put_reg(op->load_store_multiple.base_reg, temp_addr2);
	}

	// see if we need to move spsr into cpsr
	if(op->flags & UOPLSMFLAGS_LOAD_CPSR) {
		reg_t spsr = cpu.spsr; // save it here because cpu.spsr might change in set_cpu_mode()
		set_cpu_mode(cpu.spsr & PSR_MODE_MASK);
		cpu.cpsr = spsr;
	}

#if COUNT_CYCLES
	// cycle count
	if(get_core() == ARM7) {
		add_to_perf_counter(CYCLE_COUNT, op->load_store_multiple.reg_count + 1);
		if(op->load_store_multiple.reg_bitmap & 0x8000) // loaded into PC
			add_to_perf_counter(CYCLE_COUNT, 2);
	} else /* if(get_core() >= ARM9) */ {
		add_to_perf_counter(CYCLE_COUNT, (op->load_store_multiple.reg_count > 1) ? (op->load_store_multiple.reg_count - 1) : 1);
		if(op->load_store_multiple.reg_bitmap & 0x8000) {
			add_to_perf_counter(CYCLE_COUNT, 4);
			if(get_core() == ARM9e && op->load_store_multiple.reg_count == 0)
				add_to_perf_counter(CYCLE_COUNT, -1); // ldm of just pc is one cycle faster on ARM9e
		}
	}				
#endif
#if COUNT_ARM_OPS
	inc_perf_counter(OP_LOAD);
#endif
}

static inline __ALWAYS_INLINE void uop_load_multiple_s(struct uop *op)
{
	armaddr_t temp_addr, temp_addr2;
	word temp_word;
	int i;
	word reg_list = op->load_store_multiple.reg_bitmap;

	// calculate the base address
	temp_addr = get_reg(op->load_store_multiple.base_reg);
	temp_addr2 = temp_addr + op->load_store_multiple.base_offset;

	// r15 cannot be in the register list, would have resulted in a different instruction
	ASSERT((reg_list & 0x8000) == 0);

	// scan through the list of registers, reading in each one
	ASSERT((reg_list >> 16) == 0);
	for(i = 0; reg_list != 0; i++, reg_list >>= 1) {
		if(reg_list & 1) {
			if(mmu_read_mem_word(temp_addr2, &temp_word)) {
				// there was a data abort, and we may have trashed the base register. Restore it.
				put_reg_user(op->load_store_multiple.base_reg, temp_addr);
				return;
			}
			put_reg_user(i, temp_word);
			temp_addr2 += 4;
		}
	}

	// writeback
	// NOTE: writeback with the S bit set is unpredictable
	if(op->flags & UOPLSMFLAGS_WRITEBACK) {
		temp_addr2 = temp_addr + op->load_store_multiple.writeback_offset;
		put_reg(op->load_store_multiple.base_reg, temp_addr2);
	}

#if COUNT_CYCLES
	// cycle count
	if(get_core() == ARM7) {
		add_to_perf_counter(CYCLE_COUNT, op->load_store_multiple.reg_count + 1);
		if(op->load_store_multiple.reg_bitmap & 0x8000) // loaded into PC
			add_to_perf_counter(CYCLE_COUNT, 2);
	} else /* if(get_core() >= ARM9) */ {
		add_to_perf_counter(CYCLE_COUNT, (op->load_store_multiple.reg_count > 1) ? (op->load_store_multiple.reg_count - 1) : 1);
		if(op->load_store_multiple.reg_bitmap & 0x8000) {
			add_to_perf_counter(CYCLE_COUNT, 4);
			if(get_core() == ARM9e && op->load_store_multiple.reg_count == 0)
				add_to_perf_counter(CYCLE_COUNT, -1); // ldm of just pc is one cycle faster on ARM9e
		}
	}				
#endif
#if COUNT_ARM_OPS
	inc_perf_counter(OP_LOAD);
#endif
}

static inline __ALWAYS_INLINE void uop_store_multiple(struct uop *op)
{
	armaddr_t temp_addr, temp_addr2;
	int i;
	word reg_list = op->load_store_multiple.reg_bitmap;

	// calculate the base address
	temp_addr = get_reg(op->load_store_multiple.base_reg);
	temp_addr2 = temp_addr + op->load_store_multiple.base_offset;

	// scan through the list of registers, storing each one
	ASSERT((reg_list >> 16) == 0);
	for(i = 0; reg_list != 0; i++, reg_list >>= 1) {
		if(reg_list & 1) {
			if(mmu_write_mem_word(temp_addr2, get_reg(i)))
				return; // data abort
			temp_addr2 += 4;
		}
	}

	// writeback
	if(op->flags & UOPLSMFLAGS_WRITEBACK) {
		temp_addr2 = temp_addr + op->load_store_multiple.writeback_offset;
		put_reg(op->load_store_multiple.base_reg, temp_addr2);
	}

#if COUNT_CYCLES
	// cycle count
	if(get_core() == ARM7) {
		add_to_perf_counter(CYCLE_COUNT, (op->load_store_multiple.reg_count - 1) + 1);
	} else /* if(get_core() >= ARM9) */ {
		add_to_perf_counter(CYCLE_COUNT, (op->load_store_multiple.reg_count > 1) ? (op->load_store_multiple.reg_count - 1) : 1);
	}
#endif
#if COUNT_ARM_OPS
	inc_perf_counter(OP_STORE);
#endif
}

static inline __ALWAYS_INLINE void uop_store_multiple_s(struct uop *op)
{
	armaddr_t temp_addr, temp_addr2;
	int i;
	word reg_list = op->load_store_multiple.reg_bitmap;

	// calculate the base address
	temp_addr = get_reg(op->load_store_multiple.base_reg);
	temp_addr2 = temp_addr + op->load_store_multiple.base_offset;

	// scan through the list of registers, storing each one
	ASSERT((reg_list >> 16) == 0);
	for(i = 0; reg_list != 0; i++, reg_list >>= 1) {
		if(reg_list & 1) {
			if(mmu_write_mem_word(temp_addr2, get_reg_user(i)))
				return; // data abort
			temp_addr2 += 4;
		}
	}

	// writeback
	// NOTE: writeback with the S bit set is unpredictable
	if(op->flags & UOPLSMFLAGS_WRITEBACK) {
		temp_addr2 = temp_addr + op->load_store_multiple.writeback_offset;
		put_reg(op->load_store_multiple.base_reg, temp_addr2);
	}

#if COUNT_CYCLES
	// cycle count
	if(get_core() == ARM7) {
		add_to_perf_counter(CYCLE_COUNT, (op->load_store_multiple.reg_count - 1) + 1);
	} else /* if(get_core() >= ARM9) */ {
		add_to_perf_counter(CYCLE_COUNT, (op->load_store_multiple.reg_count > 1) ? (op->load_store_multiple.reg_count - 1) : 1);
	}
#endif
#if COUNT_ARM_OPS
	inc_perf_counter(OP_STORE);
#endif
}

// generic data process with immediate operand, no S bit, PC may be target
static inline __ALWAYS_INLINE void uop_data_processing_imm(struct uop *op)
{
	word immediate = op->data_processing_imm.immediate;
	word temp_word = get_reg(op->data_processing_imm.source_reg);

	ASSERT(op->data_processing_imm.dp_opcode < 16);
	ASSERT_VALID_REG(op->data_processing_imm.source_reg);
	ASSERT_VALID_REG(op->data_processing_imm.dest_reg);

	DATA_PROCESSING_OP_TABLE_NOFLAGS(op->data_processing_imm.dp_opcode,
		temp_word, // result
		temp_word, immediate); // a & b

	// write the result out
	// NOTE: if the op was originally one of the four arm test ops
	// (TST, TEQ, CMP, CMN), we would be using the DATA_PROCESSING_IMM_S
	// instruction form, since not having writeback makes no sense
	put_reg(op->data_processing_imm.dest_reg, temp_word);

#if COUNT_ARM_OPS
	inc_perf_counter(OP_DATA_PROC);
#endif
#if COUNT_ARITH_UOPS
	inc_perf_counter(UOP_ARITH_OPCODE + op->data_processing_imm.dp_opcode);
#endif
}

// generic data processing with register operand, no S bit, PC may be target
static inline __ALWAYS_INLINE void uop_data_processing_reg(struct uop *op)
{
	word temp_word = get_reg(op->data_processing_reg.source_reg);
	word operand2 = get_reg(op->data_processing_reg.source2_reg);
	
	ASSERT(op->data_processing_reg.dp_opcode < 16);
	ASSERT_VALID_REG(op->data_processing_reg.source_reg);
	ASSERT_VALID_REG(op->data_processing_reg.source2_reg);
	ASSERT_VALID_REG(op->data_processing_reg.dest_reg);

	DATA_PROCESSING_OP_TABLE_NOFLAGS(op->data_processing_reg.dp_opcode,
		temp_word, // result
		temp_word, operand2); // a & b

	// write the result out
	// NOTE: if the op was originally one of the four arm test ops
	// (TST, TEQ, CMP, CMN), we would be using the DATA_PROCESSING_REG_S
	// instruction form, since not having writeback makes no sense
	put_reg(op->data_processing_reg.dest_reg, temp_word);

#if COUNT_ARM_OPS
	inc_perf_counter(OP_DATA_PROC);
#endif
#if COUNT_ARITH_UOPS
	inc_perf_counter(UOP_ARITH_OPCODE + op->data_processing_reg.dp_opcode);
#endif
}

// generic data process with immediate operand, S bit, PC may be target
static inline __ALWAYS_INLINE void uop_data_processing_imm_s(struct uop *op)
{
	bool Rd_writeback;
	bool arith_op;
	word immediate = op->data_processing_imm.immediate;
	int carry, ovl;
	
	word temp_word = get_reg(op->data_processing_imm.source_reg);
	
	ASSERT(op->data_processing_imm.dp_opcode < 16);
	ASSERT_VALID_REG(op->data_processing_imm.source_reg);
	ASSERT_VALID_REG(op->data_processing_imm.dest_reg);

	Rd_writeback = TRUE;
	arith_op = FALSE;
	carry = 0;
	ovl = 0;
	DATA_PROCESSING_OP_TABLE(op->data_processing_imm.dp_opcode, 
		temp_word, // result
		temp_word, immediate, // a & b
		arith_op, Rd_writeback, carry, ovl);

	if(Rd_writeback)
		put_reg(op->data_processing_imm.dest_reg, temp_word);

	if(op->data_processing_imm.dest_reg != PC) {
		set_NZ_condition(temp_word);
		if(arith_op) {
			set_condition(PSR_CC_CARRY, carry);
			set_condition(PSR_CC_OVL, ovl);
		} else {
			// carry out from the shifter depending on how it was precalculated
			if(op->flags & UOPDPFLAGS_SET_CARRY_FROM_SHIFTER)
				set_condition(PSR_CC_CARRY, op->flags & UOPDPFLAGS_CARRY_FROM_SHIFTER);
		}
	} else {
		// destination was pc, and S bit was set, this means we swap spsr
		reg_t spsr = cpu.spsr; // save it here because cpu.spsr might change in set_cpu_mode()

		// see if we're about to switch thumb state
		if((spsr & PSR_THUMB) != (cpu.cpsr & PSR_THUMB))
			cpu.curr_cp = NULL; // force a codepage reload

		set_cpu_mode(cpu.spsr & PSR_MODE_MASK);
		cpu.cpsr = spsr;
	}

#if COUNT_ARM_OPS
	inc_perf_counter(OP_DATA_PROC);
#endif
#if COUNT_ARITH_UOPS
	inc_perf_counter(UOP_ARITH_OPCODE + op->data_processing_imm.dp_opcode);
#endif
}

// generic data processing with register operand, S bit, PC may be target
static inline __ALWAYS_INLINE void uop_data_processing_reg_s(struct uop *op) 
{
	bool Rd_writeback;
	bool arith_op;
	int carry, ovl;
	
	word temp_word = get_reg(op->data_processing_reg.source_reg);
	word temp_word2 = get_reg(op->data_processing_reg.source2_reg);
	
	ASSERT(op->data_processing_reg.dp_opcode < 16);
	ASSERT_VALID_REG(op->data_processing_reg.source_reg);
	ASSERT_VALID_REG(op->data_processing_reg.source2_reg);
	ASSERT_VALID_REG(op->data_processing_reg.dest_reg);

	Rd_writeback = TRUE;
	arith_op = FALSE;
	carry = 0;
	ovl = 0;
	DATA_PROCESSING_OP_TABLE(op->data_processing_reg.dp_opcode, 
		temp_word, // result
		temp_word, temp_word2, // a & b
		arith_op, Rd_writeback, carry, ovl);

	if(Rd_writeback)
		put_reg(op->data_processing_reg.dest_reg, temp_word);

	if(op->data_processing_reg.dest_reg != PC) {
		set_NZ_condition(temp_word);
		if(arith_op) {
			set_condition(PSR_CC_CARRY, carry);
			set_condition(PSR_CC_OVL, ovl);
		}
	} else {
		// destination was pc, and S bit was set, this means we swap spsr
		reg_t spsr = cpu.spsr; // save it here because cpu.spsr might change in set_cpu_mode()

		// see if we're about to switch thumb state
		if((spsr & PSR_THUMB) != (cpu.cpsr & PSR_THUMB))
			cpu.curr_cp = NULL; // force a codepage reload

		set_cpu_mode(cpu.spsr & PSR_MODE_MASK);
		cpu.cpsr = spsr;
	}

#if COUNT_ARM_OPS
	inc_perf_counter(OP_DATA_PROC);
#endif
#if COUNT_ARITH_UOPS
	inc_perf_counter(UOP_ARITH_OPCODE + op->data_processing_reg.dp_opcode);
#endif
}

// generic data processing with immediate barrel shifter, no S bit, PC may be target
static inline __ALWAYS_INLINE void uop_data_processing_imm_shift(struct uop *op)
{
	bool Rd_writeback;
	bool arith_op;
	int carry, ovl;
	bool shifter_carry_out;
	word shifter_operand;
	word shift_imm;

	ASSERT(op->data_processing_imm_shift.shift_opcode < 4);
	ASSERT(op->data_processing_imm_shift.dp_opcode < 16);
	ASSERT_VALID_REG(op->data_processing_imm_shift.source_reg);
	ASSERT_VALID_REG(op->data_processing_imm_shift.source2_reg);
	ASSERT_VALID_REG(op->data_processing_imm_shift.dest_reg);

	// first operand
	word temp_word = get_reg(op->data_processing_imm_shift.source_reg);
	word temp_word2 = get_reg(op->data_processing_imm_shift.source2_reg);
	shift_imm = op->data_processing_imm_shift.shift_imm;

	// handle the immediate shift form of barrel shifter
	switch(op->data_processing_imm_shift.shift_opcode) {
		default: case 0: // LSL
			if(shift_imm == 0) {
				// shouldn't see this form, it would have been factored out into a simpler instruction
				shifter_operand = temp_word2;
				shifter_carry_out = get_condition(PSR_CC_CARRY);
			} else {
				shifter_operand = LSL(temp_word2, shift_imm);
				shifter_carry_out = BIT(temp_word2, 32 - shift_imm);
			}
			break;
		case 1: // LSR
			if(shift_imm == 0) {
				shifter_operand = 0;
				shifter_carry_out = BIT(temp_word2, 31);
			} else {
				shifter_operand = LSR(temp_word2, shift_imm);
				shifter_carry_out = BIT(temp_word2, shift_imm - 1);
			}
			break;
		case 2: // ASR
			if(shift_imm == 0) {
				if(BIT(temp_word2, 31) == 0) {
					shifter_operand = 0;
					shifter_carry_out = 0; // Rm[31] == 0
				} else {
					shifter_operand = 0xffffffff;
					shifter_carry_out = 0x80000000; // Rm[31] == 1
				}
			} else {
				shifter_operand = ASR(temp_word2, shift_imm);
				shifter_carry_out = BIT(temp_word2, shift_imm - 1);
			}
			break;
		case 3: // ROR
			if(shift_imm == 0) {
				// RRX
				shifter_operand = (get_condition(PSR_CC_CARRY) ? 0x80000000: 0) | LSR(temp_word2, 1);
				shifter_carry_out = BIT(temp_word2, 0);
			} else {
				shifter_operand = ROR(temp_word2, shift_imm);
				shifter_carry_out = BIT(temp_word2, shift_imm - 1);
			}
			break;
	}

	// do the op
	Rd_writeback = TRUE;
	arith_op = FALSE;
	carry = 0;
	ovl = 0;
	DATA_PROCESSING_OP_TABLE(op->data_processing_imm_shift.dp_opcode, 
		temp_word, // result
		temp_word, shifter_operand, // a & b
		arith_op, Rd_writeback, carry, ovl);

	if(Rd_writeback)
		put_reg(op->data_processing_imm_shift.dest_reg, temp_word);

	if(op->flags & UOPDPFLAGS_S_BIT) {
		if(op->data_processing_imm_shift.dest_reg != PC) {
			set_NZ_condition(temp_word);
			if(arith_op) {
				set_condition(PSR_CC_CARRY, carry);
				set_condition(PSR_CC_OVL, ovl);
			} else {
				// carry out from the shifter
				set_condition(PSR_CC_CARRY, shifter_carry_out);
			}
		} else {
			// destination was pc, and S bit was set, this means we swap spsr
			reg_t spsr = cpu.spsr; // save it here because cpu.spsr might change in set_cpu_mode()

			// see if we're about to switch thumb state
			if((spsr & PSR_THUMB) != (cpu.cpsr & PSR_THUMB))
				cpu.curr_cp = NULL; // force a codepage reload
	
			set_cpu_mode(cpu.spsr & PSR_MODE_MASK);
			cpu.cpsr = spsr;
		}
	}

#if COUNT_ARM_OPS
	inc_perf_counter(OP_DATA_PROC);
#endif
#if COUNT_ARITH_UOPS
	inc_perf_counter(UOP_ARITH_OPCODE + op->data_processing_imm_shift.dp_opcode);
#endif
}	

// generic data processing with register based barrel shifter, no S bit, PC may be target
static inline __ALWAYS_INLINE void uop_data_processing_reg_shift(struct uop *op) 
	{
	bool Rd_writeback;
	bool arith_op;
	int carry, ovl;
	bool shifter_carry_out;
	word shifter_operand;

	ASSERT(op->data_processing_reg_shift.shift_opcode < 4);
	ASSERT(op->data_processing_reg_shift.dp_opcode < 16);
	ASSERT_VALID_REG(op->data_processing_reg_shift.source_reg);
	ASSERT_VALID_REG(op->data_processing_reg_shift.source2_reg);
	ASSERT_VALID_REG(op->data_processing_reg_shift.dest_reg);

	// operands
	word temp_word = get_reg(op->data_processing_reg_shift.source_reg);
	word temp_word2 = get_reg(op->data_processing_reg_shift.source2_reg);
	word temp_word3 = get_reg(op->data_processing_reg_shift.shift_reg);
	
	// we only care about the bottom 8 bits of Rs
	temp_word3 = BITS(temp_word3, 7, 0);

	// handle the immediate shift form of barrel shifter
	switch(op->data_processing_reg_shift.shift_opcode) {
		default: case 0: // LSL by reg (page A5-10)
			if(temp_word3 == 0) {
				shifter_operand = temp_word2;
				shifter_carry_out = get_condition(PSR_CC_CARRY);
			} else if(temp_word3 < 32) {
				word lower_shift = BITS(temp_word3, 7, 0);
				shifter_operand = LSL(temp_word2, lower_shift);
				shifter_carry_out = BIT(temp_word2, 32 - lower_shift);
			} else if(temp_word3 == 32) {
				shifter_operand = 0;
				shifter_carry_out = BIT(temp_word2, 0);
			} else { // temp_word3 > 32
				shifter_operand = 0;
				shifter_carry_out = 0;
			}
			break;
		case 1: // LSR by reg (page A5-12)
			if(temp_word3 == 0) {
				shifter_operand = temp_word2;
				shifter_carry_out = get_condition(PSR_CC_CARRY);
			} else if(temp_word3 < 32) {
				shifter_operand = LSR(temp_word2, temp_word3);
				shifter_carry_out = BIT(temp_word2, temp_word3 - 1);
			} else if(temp_word3 == 32) {
				shifter_operand = 0;
				shifter_carry_out = BIT(temp_word2, 31);
			} else {
				shifter_operand = 0;
				shifter_carry_out = 0;
			}
			break;
		case 2: // ASR by reg (page A5-14)
			if(temp_word3 == 0) {
				shifter_operand = temp_word2;
				shifter_carry_out = get_condition(PSR_CC_CARRY);
			} else if(temp_word3 < 32) {
				shifter_operand = ASR(temp_word2, temp_word3);
				shifter_carry_out = BIT(temp_word2, temp_word3 - 1);
			} else if(temp_word3 >= 32) {
				if(BIT(temp_word2, 31) == 0) {
					shifter_operand = 0;
					shifter_carry_out = 0; // Rm[31] == 0
				} else {
					shifter_operand = 0xffffffff;
					shifter_carry_out = 0x80000000; // Rm[31] == 1
				}
			}
			break;
		case 3: // ROR by reg (page A5-16)
			if(temp_word3 == 0) {
				shifter_operand = temp_word2;
				shifter_carry_out = get_condition(PSR_CC_CARRY);
			} else if(BITS(temp_word3, 4, 0) == 0) {
				shifter_operand = temp_word2;
				shifter_carry_out = BIT(temp_word2, 31);
			} else { // temp_word3 & 0x1f > 0
				word lower_4bits = BITS(temp_word3, 4, 0);
				shifter_operand = ROR(temp_word2, lower_4bits);
				shifter_carry_out = BIT(temp_word2, lower_4bits - 1);
			}
			break;
	}

#if COUNT_CYCLES
	/* shifting by a reg value costs an extra cycle */
	add_to_perf_counter(CYCLE_COUNT, 1);
#endif
	// do the op
	Rd_writeback = TRUE;
	arith_op = FALSE;
	carry = 0;
	ovl = 0;
	DATA_PROCESSING_OP_TABLE(op->data_processing_reg_shift.dp_opcode, 
		temp_word, // result
		temp_word, shifter_operand, // a & b
		arith_op, Rd_writeback, carry, ovl);

	if(Rd_writeback)
		put_reg(op->data_processing_reg_shift.dest_reg, temp_word);

	if(op->flags & UOPDPFLAGS_S_BIT) {
		if(op->data_processing_reg_shift.dest_reg != PC) {
			set_NZ_condition(temp_word);
			if(arith_op) {
				set_condition(PSR_CC_CARRY, carry);
				set_condition(PSR_CC_OVL, ovl);
			} else {
				// carry out from the shifter
				set_condition(PSR_CC_CARRY, shifter_carry_out);
			}
		} else {
			// destination was pc, and S bit was set, this means we swap spsr
			reg_t spsr = cpu.spsr; // save it here because cpu.spsr might change in set_cpu_mode()

			// see if we're about to switch thumb state
			if((spsr & PSR_THUMB) != (cpu.cpsr & PSR_THUMB))
				cpu.curr_cp = NULL; // force a codepage reload
	
			set_cpu_mode(cpu.spsr & PSR_MODE_MASK);
			cpu.cpsr = spsr;
		}
	}

#if COUNT_ARM_OPS
	inc_perf_counter(OP_DATA_PROC);
#endif
#if COUNT_ARITH_UOPS
	inc_perf_counter(UOP_ARITH_OPCODE + op->data_processing_reg_shift.dp_opcode);
#endif
}

// simple load of immediate into register, PC may not be target
static inline __ALWAYS_INLINE void uop_mov_imm(struct uop *op) 
{
	put_reg_nopc(op->simple_dp_imm.dest_reg, op->simple_dp_imm.immediate);

#if COUNT_ARM_OPS
	inc_perf_counter(OP_DATA_PROC);
#endif
}

// simple load of immediate into register, set N and Z flags, PC may not be target
static inline __ALWAYS_INLINE void uop_mov_imm_nz(struct uop *op) 
{
	put_reg_nopc(op->simple_dp_imm.dest_reg, op->simple_dp_imm.immediate);
	set_NZ_condition(op->simple_dp_imm.immediate);

#if COUNT_ARM_OPS
	inc_perf_counter(OP_DATA_PROC);
#endif
}

// simple mov from register to register, PC may not be target
static inline __ALWAYS_INLINE void uop_mov_reg(struct uop *op) 
{
	put_reg_nopc(op->simple_dp_reg.dest_reg, get_reg(op->simple_dp_reg.source2_reg));

#if COUNT_ARM_OPS
	inc_perf_counter(OP_DATA_PROC);
#endif
}

// simple compare of register to immediate value
static inline __ALWAYS_INLINE void uop_cmp_imm_s(struct uop *op) 
{
	int carry, ovl;
	word result;
	
	// subtract the immediate from the source register
	result = do_add(get_reg(op->simple_dp_imm.source_reg), ~(op->simple_dp_imm.immediate), 1, &carry, &ovl);

	// set flags on the result
	set_NZ_condition(result);
	set_condition(PSR_CC_CARRY, carry);
	set_condition(PSR_CC_OVL, ovl);

#if COUNT_ARM_OPS
	inc_perf_counter(OP_DATA_PROC);
#endif
}

// simple compare of two registers
static inline __ALWAYS_INLINE void uop_cmp_reg_s(struct uop *op) 
{
	int carry, ovl;
	word result;

	// subtract the source2 reg from the source register
	result = do_add(get_reg(op->simple_dp_reg.source_reg), ~(get_reg(op->simple_dp_reg.source2_reg)), 1, &carry, &ovl);

	// set flags on the result
	set_NZ_condition(result);
	set_condition(PSR_CC_CARRY, carry);
	set_condition(PSR_CC_OVL, ovl);

#if COUNT_ARM_OPS
	inc_perf_counter(OP_DATA_PROC);
#endif
}

// simple negative compare of two registers
static inline __ALWAYS_INLINE void uop_cmn_reg_s(struct uop *op) 
{
	int carry, ovl;
	word result;
	
	// subtract the source2 reg from the source register
	result = do_add(get_reg(op->simple_dp_reg.source_reg), get_reg(op->simple_dp_reg.source2_reg), 0, &carry, &ovl);

	// set flags on the result
	set_NZ_condition(result);
	set_condition(PSR_CC_CARRY, carry);
	set_condition(PSR_CC_OVL, ovl);

#if COUNT_ARM_OPS
	inc_perf_counter(OP_DATA_PROC);
#endif
}

// bit test of two registers
static inline __ALWAYS_INLINE void uop_tst_reg_s(struct uop *op) 
{
	word result;
	word a;
	word b;
	
	a = get_reg(op->simple_dp_reg.source_reg);
	b = get_reg(op->simple_dp_reg.source2_reg);
	
	result = a & b;

	// set flags on the result
	set_NZ_condition(result);

#if COUNT_ARM_OPS
	inc_perf_counter(OP_DATA_PROC);
#endif
}

// simple add of immediate to register, PC may not be target
static inline __ALWAYS_INLINE void uop_add_imm(struct uop *op) 
{
	word a;
	word result;

	a = get_reg(op->simple_dp_imm.source_reg);
	result = a + op->simple_dp_imm.immediate;
	put_reg_nopc(op->simple_dp_imm.dest_reg, result);

#if COUNT_ARM_OPS
	inc_perf_counter(OP_DATA_PROC);
#endif
}

// simple add of immediate to register, S bit, PC may not be target
static inline __ALWAYS_INLINE void uop_add_imm_s(struct uop *op) 
{
	int carry, ovl;
	word a;
	word result;

	a = get_reg(op->simple_dp_imm.source_reg);
	result = do_add(a, op->simple_dp_imm.immediate, 0, &carry, &ovl);
	put_reg_nopc(op->simple_dp_imm.dest_reg, result);

	// set flags on the result
	set_NZ_condition(result);
	set_condition(PSR_CC_CARRY, carry);
	set_condition(PSR_CC_OVL, ovl);

#if COUNT_ARM_OPS
	inc_perf_counter(OP_DATA_PROC);
#endif
}

// simple add of two registers, PC may not be target
static inline __ALWAYS_INLINE void uop_add_reg(struct uop *op) 
{
	word a;
	word b;
	word result;

	a = get_reg(op->simple_dp_reg.source_reg);
	b = get_reg(op->simple_dp_reg.source2_reg);
	result = a + b;
	put_reg_nopc(op->simple_dp_reg.dest_reg, result);

#if COUNT_ARM_OPS
	inc_perf_counter(OP_DATA_PROC);
#endif
}

// simple add of two registers, S bit, PC may not be target
static inline __ALWAYS_INLINE void uop_add_reg_s(struct uop *op) 
{
	int carry, ovl;
	word a;
	word b;
	word result;

	a = get_reg(op->simple_dp_reg.source_reg);
	b = get_reg(op->simple_dp_reg.source2_reg);
	result = do_add(a, b, 0, &carry, &ovl);
	put_reg_nopc(op->simple_dp_reg.dest_reg, result);

	// set flags on the result
	set_NZ_condition(result);
	set_condition(PSR_CC_CARRY, carry);
	set_condition(PSR_CC_OVL, ovl);

#if COUNT_ARM_OPS
	inc_perf_counter(OP_DATA_PROC);
#endif
}

// simple add with carry of two registers, S bit, PC may not be target
static inline __ALWAYS_INLINE void uop_adc_reg_s(struct uop *op) 
{
	int carry, ovl;
	word a;
	word b;
	word result;

	a = get_reg(op->simple_dp_reg.source_reg);
	b = get_reg(op->simple_dp_reg.source2_reg);
	result = do_add(a, b, get_condition(PSR_CC_CARRY) ? 1 : 0, &carry, &ovl);
	put_reg_nopc(op->simple_dp_reg.dest_reg, result);

	// set flags on the result
	set_NZ_condition(result);
	set_condition(PSR_CC_CARRY, carry);
	set_condition(PSR_CC_OVL, ovl);

#if COUNT_ARM_OPS
	inc_perf_counter(OP_DATA_PROC);
#endif
}

// simple subtract of two registers, S bit, PC may not be target
static inline __ALWAYS_INLINE void uop_sub_reg_s(struct uop *op) 
{
	int carry, ovl;
	word a;
	word b;
	word result;

	a = get_reg(op->simple_dp_reg.source_reg);
	b = get_reg(op->simple_dp_reg.source2_reg);
	result = do_add(a, ~b, 1, &carry, &ovl);
	put_reg_nopc(op->simple_dp_reg.dest_reg, result);

	// set flags on the result
	set_NZ_condition(result);
	set_condition(PSR_CC_CARRY, carry);
	set_condition(PSR_CC_OVL, ovl);
#if COUNT_ARM_OPS
	inc_perf_counter(OP_DATA_PROC);
#endif
}

// simple subtract with carry of two registers, S bit, PC may not be target
static inline __ALWAYS_INLINE void uop_sbc_reg_s(struct uop *op) 
{
	int carry, ovl;
	word a;
	word b;
	word result;

	a = get_reg(op->simple_dp_reg.source_reg);
	b = get_reg(op->simple_dp_reg.source2_reg);
	result = do_add(a, ~b, get_condition(PSR_CC_CARRY) ? 1 : 0, &carry, &ovl);
	put_reg(op->simple_dp_reg.dest_reg, result);

	// set flags on the result
	set_NZ_condition(result);
	set_condition(PSR_CC_CARRY, carry);
	set_condition(PSR_CC_OVL, ovl);
#if COUNT_ARM_OPS
	inc_perf_counter(OP_DATA_PROC);
#endif
}

// or with immediate, PC may not be target
static inline __ALWAYS_INLINE void uop_orr_imm(struct uop *op) 
{
	word a;
	word result;

	a = get_reg(op->simple_dp_imm.source_reg);
	result = a | op->simple_dp_imm.immediate;

	put_reg_nopc(op->simple_dp_imm.dest_reg, result);

#if COUNT_ARM_OPS
	inc_perf_counter(OP_DATA_PROC);
#endif
}

// or by register, S bit, PC may not be target
static inline __ALWAYS_INLINE void uop_orr_reg_s(struct uop *op) 
{
	word a;
	word b;
	word result;

	a = get_reg(op->simple_dp_reg.source_reg);
	b = get_reg(op->simple_dp_reg.source2_reg);
	result = a | b;

	set_NZ_condition(result);
	put_reg_nopc(op->simple_dp_reg.dest_reg, result);

#if COUNT_ARM_OPS
	inc_perf_counter(OP_DATA_PROC);
#endif
}

// shift left of register by immediate, PC may not be target
static inline __ALWAYS_INLINE void uop_lsl_imm(struct uop *op) 
{
	word a;
	word shift;
	word result;

	a = get_reg(op->simple_dp_imm.source_reg);
	shift = op->simple_dp_imm.immediate;

	result = LSL(a, shift);

	put_reg_nopc(op->simple_dp_imm.dest_reg, result);

#if COUNT_ARM_OPS
	inc_perf_counter(OP_DATA_PROC);
#endif
}

// shift left of register by immediate, S bit, PC may not be target
static inline __ALWAYS_INLINE void uop_lsl_imm_s(struct uop *op) 
{
	int carry;
	word a;
	word immed;
	word result;

	a = get_reg(op->simple_dp_imm.source_reg);
	immed = op->simple_dp_imm.immediate;

	if(immed != 0) {
		carry = BIT(a, 32 - immed);
		result = LSL(a, immed);
	} else {
		carry = get_condition(PSR_CC_CARRY);
		result = a;
	}

	set_NZ_condition(result);
	set_condition(PSR_CC_CARRY, carry);
	put_reg_nopc(op->simple_dp_imm.dest_reg, result);

#if COUNT_ARM_OPS
	inc_perf_counter(OP_DATA_PROC);
#endif
}

// shift left of register by register, PC may not be target
static inline __ALWAYS_INLINE void uop_lsl_reg(struct uop *op) 
{
	word a;
	word shift;
	word result;

	a = get_reg(op->simple_dp_reg.source_reg);
	shift = get_reg(op->simple_dp_reg.source2_reg);
	shift &= 0xff;

	result = LSL(a, shift);

	put_reg_nopc(op->simple_dp_reg.dest_reg, result);

#if COUNT_ARM_OPS
	inc_perf_counter(OP_DATA_PROC);
#endif
}

// shift left of register by register, S bit, PC may not be target
static inline __ALWAYS_INLINE void uop_lsl_reg_s(struct uop *op) 
{
	int carry;
	word a;
	word shift;
	word result;

	a = get_reg(op->simple_dp_reg.source_reg);
	shift = get_reg(op->simple_dp_reg.source2_reg);
	shift &= 0xff;

	result = LSL(a, shift);
	if(shift == 0) {
		carry = get_condition(PSR_CC_CARRY);
	} else if(shift < 32) {
		carry = BIT(a, 32 - shift);
	} else if(shift == 32) {
		carry = BIT(a, 0);
	} else {
		carry = 0;
	}

	set_NZ_condition(result);
	set_condition(PSR_CC_CARRY, carry);
	put_reg_nopc(op->simple_dp_reg.dest_reg, result);

#if COUNT_ARM_OPS
	inc_perf_counter(OP_DATA_PROC);
#endif
}

// logical shift right by immediate, PC may not be target
static inline __ALWAYS_INLINE void uop_lsr_imm(struct uop *op) 
{
	word a;
	word immed;
	word result;

	a = get_reg(op->simple_dp_imm.source_reg);
	immed = op->simple_dp_imm.immediate;

	result = LSR(a, immed);

	put_reg_nopc(op->simple_dp_imm.dest_reg, result);

#if COUNT_ARM_OPS
	inc_perf_counter(OP_DATA_PROC);
#endif
}

// logical shift right by immediate, S bit, PC may not be target
static inline __ALWAYS_INLINE void uop_lsr_imm_s(struct uop *op) 
{
	int carry;
	word a;
	word immed;
	word result;

	a = get_reg(op->simple_dp_imm.source_reg);
	immed = op->simple_dp_imm.immediate;

	if(immed != 0) {
		carry = BIT(a, immed - 1);
		result = LSR(a, immed);
	} else {
		carry = BIT(a, 31);
		result = 0;
	}

	set_NZ_condition(result);
	set_condition(PSR_CC_CARRY, carry);
	put_reg_nopc(op->simple_dp_imm.dest_reg, result);

#if COUNT_ARM_OPS
	inc_perf_counter(OP_DATA_PROC);
#endif
}

// logical shift right by register, PC may not be target
static inline __ALWAYS_INLINE void uop_lsr_reg(struct uop *op) 
{
	word a;
	word shift;
	word result;

	a = get_reg(op->simple_dp_reg.source_reg);
	shift = get_reg(op->simple_dp_reg.source2_reg);
	shift &= 0xff;

	result = LSR(a, shift);

	put_reg_nopc(op->simple_dp_reg.dest_reg, result);

#if COUNT_ARM_OPS
	inc_perf_counter(OP_DATA_PROC);
#endif
}

// logical shift right by register, S bit, PC may not be target
static inline __ALWAYS_INLINE void uop_lsr_reg_s(struct uop *op) 
{
	int carry;
	word a;
	word shift;
	word result;

	a = get_reg(op->simple_dp_reg.source_reg);
	shift = get_reg(op->simple_dp_reg.source2_reg);
	shift &= 0xff;

	result = LSR(a, shift);
	if(shift == 0) {
		carry = get_condition(PSR_CC_CARRY);
	} else if(shift < 32) {
		carry = BIT(a, shift - 1);
	} else if(shift == 32) {
		carry = BIT(a, 31);
	} else {
		carry = 0;
	}

	set_NZ_condition(result);
	set_condition(PSR_CC_CARRY, carry);
	put_reg_nopc(op->simple_dp_reg.dest_reg, result);

#if COUNT_ARM_OPS
	inc_perf_counter(OP_DATA_PROC);
#endif
}

// arithmetic shift right by immediate,  PC may not be target
static inline __ALWAYS_INLINE void uop_asr_imm(struct uop *op) 
{
	word a;
	word immed;
	word result;

	a = get_reg(op->simple_dp_imm.source_reg);
	immed = op->simple_dp_imm.immediate;

	result = ASR(a, immed);

	put_reg_nopc(op->simple_dp_imm.dest_reg, result);

#if COUNT_ARM_OPS
	inc_perf_counter(OP_DATA_PROC);
#endif
}

// arithmetic shift right by immediate, S bit, PC may not be target
static inline __ALWAYS_INLINE void uop_asr_imm_s(struct uop *op) 
{
	int carry;
	word a;
	word immed;
	word result;

	a = get_reg(op->simple_dp_imm.source_reg);
	immed = op->simple_dp_imm.immediate;

	if(immed == 0) {
		carry = BIT(a, 31);
		if(carry)
			result = 0;
		else
			result = 0xffffffff;
	} else {
		carry = BIT(a, immed - 1);
		result = ASR(a, immed);
	}

	set_NZ_condition(result);
	set_condition(PSR_CC_CARRY, carry);
	put_reg_nopc(op->simple_dp_imm.dest_reg, result);

#if COUNT_ARM_OPS
	inc_perf_counter(OP_DATA_PROC);
#endif
}

// arithmetic shift right by register, S bit, PC may not be target
static inline __ALWAYS_INLINE void uop_asr_reg(struct uop *op) 
{
	word a;
	word shift;
	word result;

	a = get_reg(op->simple_dp_reg.source_reg);
	shift = get_reg(op->simple_dp_reg.source2_reg);

	result = ASR(a, shift);

	put_reg_nopc(op->simple_dp_reg.dest_reg, result);

#if COUNT_ARM_OPS
	inc_perf_counter(OP_DATA_PROC);
#endif
}

// arithmetic shift right by register, S bit, PC may not be target
static inline __ALWAYS_INLINE void uop_asr_reg_s(struct uop *op) 
{
	int carry;
	word a;
	word shift;
	word result;

	a = get_reg(op->simple_dp_reg.source_reg);
	shift = get_reg(op->simple_dp_reg.source2_reg);

	result = ASR(a, shift);
	if(shift == 0) {
		carry = get_condition(PSR_CC_CARRY);
	} else if(shift < 32) {
		carry = BIT(a, shift - 1);
	} else { // RmRsval >= 32
		carry = BIT(a, 31);
		if(carry)
			result = 0xffffffff;
		else
			result = 0;
	}

	set_NZ_condition(result);
	set_condition(PSR_CC_CARRY, carry);
	put_reg_nopc(op->simple_dp_reg.dest_reg, result);

#if COUNT_ARM_OPS
	inc_perf_counter(OP_DATA_PROC);
#endif
}

// rotate right by register, PC may not be target
static inline __ALWAYS_INLINE void uop_ror_reg(struct uop *op) 
{
	word a;
	word rotate;
	word result;

	a = get_reg(op->simple_dp_reg.source_reg);
	rotate = get_reg(op->simple_dp_reg.source2_reg);

	result = ROR(a, rotate);

	put_reg_nopc(op->simple_dp_reg.dest_reg, result);

#if COUNT_ARM_OPS
	inc_perf_counter(OP_DATA_PROC);
#endif
}

// rotate right by register, S bit, PC may not be target
static inline __ALWAYS_INLINE void uop_ror_reg_s(struct uop *op) 
{
	int carry;
	word a;
	word rotate;
	word rotate_lower_4_bits;
	word result;

	a = get_reg(op->simple_dp_reg.source_reg);
	rotate = get_reg(op->simple_dp_reg.source2_reg);
	rotate_lower_4_bits = BITS(rotate, 4, 0);

	if(BITS(rotate, 7, 0) == 0) {
		carry = get_condition(PSR_CC_CARRY);
		result = a; // result is unaffected
	} else if(rotate_lower_4_bits == 0) {
		carry = BIT(a, 31);
		result = a; // result is unaffected
	} else {
		carry = BIT(a, rotate_lower_4_bits - 1);
		result = ROR(a, rotate_lower_4_bits);
	}

	set_NZ_condition(result);
	set_condition(PSR_CC_CARRY, carry);
	put_reg_nopc(op->simple_dp_reg.dest_reg, result);

#if COUNT_ARM_OPS
	inc_perf_counter(OP_DATA_PROC);
#endif
}

// and by register, S bit, PC may not be target
static inline __ALWAYS_INLINE void uop_and_reg_s(struct uop *op) 
{
	word a;
	word b;
	word result;

	a = get_reg(op->simple_dp_reg.source_reg);
	b = get_reg(op->simple_dp_reg.source2_reg);
	result = a & b;

	set_NZ_condition(result);
	put_reg_nopc(op->simple_dp_reg.dest_reg, result);

#if COUNT_ARM_OPS
	inc_perf_counter(OP_DATA_PROC);
#endif
}

// xor by register, S bit, PC may not be target
static inline __ALWAYS_INLINE void uop_eor_reg_s(struct uop *op) 
{
	word a;
	word b;
	word result;

	a = get_reg(op->simple_dp_reg.source_reg);
	b = get_reg(op->simple_dp_reg.source2_reg);
	result = a ^ b;

	set_NZ_condition(result);
	put_reg_nopc(op->simple_dp_reg.dest_reg, result);

#if COUNT_ARM_OPS
	inc_perf_counter(OP_DATA_PROC);
#endif
}

// bit clear by register, S bit, PC may not be target
static inline __ALWAYS_INLINE void uop_bic_reg_s(struct uop *op) 
{
	word a;
	word b;
	word result;

	a = get_reg(op->simple_dp_reg.source_reg);
	b = get_reg(op->simple_dp_reg.source2_reg);
	result = a & ~b;

	set_NZ_condition(result);
	put_reg_nopc(op->simple_dp_reg.dest_reg, result);

#if COUNT_ARM_OPS
	inc_perf_counter(OP_DATA_PROC);
#endif
}

// negate register, S bit, PC may not be target
static inline __ALWAYS_INLINE void uop_neg_reg_s(struct uop *op) 
{
	int carry, ovl;
	word b;
	word result;

	b = get_reg(op->simple_dp_reg.source2_reg);
	result = do_add(0, ~b, 1, &carry, &ovl);

	set_condition(PSR_CC_CARRY, carry);
	set_condition(PSR_CC_OVL, ovl);
	set_NZ_condition(result);
	put_reg_nopc(op->simple_dp_reg.dest_reg, result);

#if COUNT_ARM_OPS
	inc_perf_counter(OP_DATA_PROC);
#endif
}

// bitwise negation register, S bit, PC may not be target
static inline __ALWAYS_INLINE void uop_mvn_reg_s(struct uop *op) 
{
	word b;
	word result;

	b = get_reg(op->simple_dp_reg.source2_reg);
	result = ~b;

	set_NZ_condition(result);
	put_reg_nopc(op->simple_dp_reg.dest_reg, result);

#if COUNT_ARM_OPS
	inc_perf_counter(OP_DATA_PROC);
#endif
}

static inline __ALWAYS_INLINE void uop_multiply(struct uop *op) 
{
	// multiply the first two operands
	word temp_word = get_reg(op->mul.source_reg);
	word temp_word2 = get_reg(op->mul.source2_reg) * temp_word;

	// add a third one conditionally
	if(op->flags & UOPMULFLAGS_ACCUMULATE)
		temp_word2 += get_reg(op->mul.accum_reg);

	// store the result
	put_reg(op->mul.dest_reg, temp_word2);

	// set the NZ bits on exit
	if(op->flags & UOPMULFLAGS_S_BIT)
		set_NZ_condition(temp_word2);

#if COUNT_CYCLES
	// cycle count
	if(get_core() <= ARM9) {
		int signed_word = temp_word;

		if ((signed_word >> 8) == 0 || (signed_word >> 8) == -1)
			add_to_perf_counter(CYCLE_COUNT, 1);
		else if ((signed_word >> 16) == 0 || (signed_word >> 16) == -1)
			add_to_perf_counter(CYCLE_COUNT, 2);
		else if ((signed_word >> 24) == 0 || (signed_word >> 24) == -1)
			add_to_perf_counter(CYCLE_COUNT, 3);
	} else /* if(get_core() == ARM9e) */ {
		/* ARM9e core can do the multiply in 2 cycles, with an interlock */
		add_to_perf_counter(CYCLE_COUNT, 1);
		// XXX schedule interlock here
	}
#endif
#if COUNT_ARM_OPS
	inc_perf_counter(OP_MUL);
#endif
}

static inline __ALWAYS_INLINE void uop_multiply_long(struct uop *op) 
{
	word reslo, reshi;
	uint64_t result;

	// get the first two operands
	word temp_word = get_reg(op->mull.source_reg);
	word temp_word2 = get_reg(op->mull.source2_reg);

	// signed or unsigned multiply
	// XXX is this correct
	if(op->flags & UOPMULFLAGS_SIGNED) {
		result = (int64_t)(int)temp_word * (int)temp_word2;
	} else {
		result = (uint64_t)temp_word * temp_word2;
	}

	// accumulate
	if(op->flags & UOPMULFLAGS_ACCUMULATE) {
		uint64_t acc = get_reg(op->mull.desthi_reg);
		acc = (acc << 32) | get_reg(op->mull.destlo_reg);
		result += acc;
	}

	// store the results
	reslo = result;
	reshi = result >> 32;
	put_reg(op->mull.destlo_reg, reslo);
	put_reg(op->mull.desthi_reg, reshi);

	// set the NZ bits on exit
	if(op->flags & UOPMULFLAGS_S_BIT) {
		set_condition(PSR_CC_NEG, BIT(reshi, 31));
		set_condition(PSR_CC_ZERO, (reslo | reshi) == 0);
	}

#if COUNT_CYCLES
	// cycle count
	if(get_core() <= ARM9) {
		if ((temp_word >> 8) == 0)
			add_to_perf_counter(CYCLE_COUNT, 2);
		else if ((temp_word >> 16) == 0)
			add_to_perf_counter(CYCLE_COUNT, 3);
		else if ((temp_word >> 24) == 0)
			add_to_perf_counter(CYCLE_COUNT, 4);
	} else /* if(get_core() == ARM9e) */ {
		/* ARM9e core can do the multiply in 3 cycles, with an interlock */
		add_to_perf_counter(CYCLE_COUNT, 2);
		// XXX schedule interlock here
	}
#endif
#if COUNT_ARM_OPS
	inc_perf_counter(OP_MUL);
#endif
}

static inline __ALWAYS_INLINE void uop_count_leading_zeros(struct uop *op)
{
	word val;
	int count;

	ASSERT_VALID_REG(op->count_leading_zeros.source_reg);
	ASSERT_VALID_REG(op->count_leading_zeros.dest_reg);

	// get the value we're supposed to count
	val = get_reg(op->count_leading_zeros.source_reg);
	
	count = clz(val);
	
	// put the result back
	put_reg(op->count_leading_zeros.dest_reg, count);

	// XXX cycle count, or is it always 1 cycle?

#if COUNT_ARM_OPS
	inc_perf_counter(OP_MISC);
#endif
}

static inline __ALWAYS_INLINE void uop_move_to_sr_imm(struct uop *op) 
{
	reg_t old_psr, new_psr;

	if(op->flags & UOPMSR_R_BIT) {
		// spsr
		old_psr = cpu.spsr;
	} else {
		// cpsr
		old_psr = cpu.cpsr;
	}
	
	word field_mask = op->move_to_sr_imm.field_mask;
	
	// if we're in user mode, we can only modify the top 8 bits
	if(!arm_in_priviledged())
		field_mask &= 0xff000000;

	// or in the new immediate value
	new_psr = (old_psr & ~field_mask) | (op->move_to_sr_imm.immediate & field_mask);

	// write the new value back
	if(op->flags & UOPMSR_R_BIT) {
		// spsr
		// NOTE: UNPREDICTABLE if the cpu is in user or system mode
		cpu.spsr = new_psr;
	} else {
		// cpsr
		set_cpu_mode(new_psr & PSR_MODE_MASK);
		cpu.cpsr = new_psr;

#if COUNT_CYCLES
		// cycle count
		if(field_mask & 0x00ffffff)
			add_to_perf_counter(CYCLE_COUNT, 2); // we updated something other than the status flags
#endif
	}

#if COUNT_ARM_OPS
	inc_perf_counter(OP_MISC);
#endif
}

static inline __ALWAYS_INLINE void uop_move_to_sr_reg(struct uop *op) 
{
	reg_t old_psr, new_psr;

	if(op->flags & UOPMSR_R_BIT) {
		// spsr
		old_psr = cpu.spsr;
	} else {
		// cpsr
		old_psr = cpu.cpsr;
	}
	
	word field_mask = op->move_to_sr_imm.field_mask;
	word temp_word = get_reg(op->move_to_sr_reg.reg);
	
	// if we're in user mode, we can only modify the top 8 bits
	if(!arm_in_priviledged())
		field_mask &= 0xff000000;

	// or in the new immediate value
	new_psr = (old_psr & ~field_mask) | (temp_word & field_mask);

	// write the new value back
	if(op->flags & UOPMSR_R_BIT) {
		// spsr
		// NOTE: UNPREDICTABLE if the cpu is in user or system mode
		cpu.spsr = new_psr;
	} else {
		// cpsr
		set_cpu_mode(new_psr & PSR_MODE_MASK);
		cpu.cpsr = new_psr;

#if COUNT_CYCLES
		// cycle count
		if(field_mask & 0x00ffffff)
			add_to_perf_counter(CYCLE_COUNT, 2); // we updated something other than the status flags
#endif
	}

#if COUNT_ARM_OPS
	inc_perf_counter(OP_MISC);
#endif
}

static inline __ALWAYS_INLINE void uop_move_from_sr(struct uop *op) 
{
	if(op->flags & UOPMSR_R_BIT) {
		// NOTE: UNPREDICTABLE if the cpu is in user or system mode
		put_reg(op->move_from_sr.reg, cpu.spsr);
	} else {
		put_reg(op->move_from_sr.reg, cpu.cpsr);
	}

#if COUNT_ARM_OPS
	inc_perf_counter(OP_MISC);
#endif
#if COUNT_CYCLES
	// 2 cycles on arm9+
	if(get_core() >= ARM9)
		add_to_perf_counter(CYCLE_COUNT, 1);
#endif
}


static inline __ALWAYS_INLINE void uop_undefined(struct uop *op) 
{
	atomic_or(&cpu.pending_exceptions, EX_UNDEFINED);
	
//	UOP_TRACE(0, "undefined instruction at 0x%x\n", get_reg(PC));

#if COUNT_CYCLES
	if(get_core() == ARM7)
		add_to_perf_counter(CYCLE_COUNT, 3);
	else /* if(get_core() >= ARM9) */
		add_to_perf_counter(CYCLE_COUNT, 2);
#endif
#if COUNT_ARM_OPS
	inc_perf_counter(OP_MISC);
#endif
}

static inline __ALWAYS_INLINE void uop_swi(struct uop *op) 
{
	atomic_or(&cpu.pending_exceptions, EX_SWI);

	// always takes 3 cycles
#if COUNT_CYCLES
	add_to_perf_counter(CYCLE_COUNT, 2);
#endif
#if COUNT_ARM_OPS
	inc_perf_counter(OP_MISC);
#endif
}

static inline __ALWAYS_INLINE void uop_bkpt(struct uop *op) 
{
	atomic_or(&cpu.pending_exceptions, EX_PREFETCH);

	// always takes 3 cycles
#if COUNT_CYCLES
	add_to_perf_counter(CYCLE_COUNT, 2);
#endif
#if COUNT_ARM_OPS
	inc_perf_counter(OP_MISC);
#endif
}

static inline __ALWAYS_INLINE void uop_coproc_reg_transfer(struct uop *op) 
{
	struct arm_coprocessor *cp = &cpu.coproc[op->coproc.cp_num];

	if(cp->installed) {
		cp->reg_transfer(op->coproc.raw_instruction, cp->data);
	} else {
		/* coprocessor not present, same as an undefined instruction */
		uop_undefined(op);
	}

#if COUNT_ARM_OPS
	inc_perf_counter(OP_COP_REG_TRANS);
#endif
}

static inline __ALWAYS_INLINE void uop_coproc_double_reg_transfer(struct uop *op) 
{
	struct arm_coprocessor *cp = &cpu.coproc[op->coproc.cp_num];

	if(cp->installed) {
		cp->double_reg_transfer(op->coproc.raw_instruction, cp->data);
	} else {
		/* coprocessor not present, same as an undefined instruction */
		uop_undefined(op);
	}

#if COUNT_ARM_OPS
	inc_perf_counter(OP_COP_REG_TRANS);
#endif
}

static inline __ALWAYS_INLINE void uop_coproc_data_processing(struct uop *op) 
{
	struct arm_coprocessor *cp = &cpu.coproc[op->coproc.cp_num];

	if(cp->installed) {
		cp->data_processing(op->coproc.raw_instruction, cp->data);
	} else {
		/* coprocessor not present, same as an undefined instruction */
		uop_undefined(op);
	}

#if COUNT_ARM_OPS
	inc_perf_counter(OP_COP_DATA_PROC);
#endif
}

static inline __ALWAYS_INLINE void uop_coproc_load_store(struct uop *op) 
{
	struct arm_coprocessor *cp = &cpu.coproc[op->coproc.cp_num];

	if(cp->installed) {
		cp->load_store(op->coproc.raw_instruction, cp->data);
	} else {
		/* coprocessor not present, same as an undefined instruction */
		uop_undefined(op);
	}

#if COUNT_ARM_OPS
	inc_perf_counter(OP_COP_LOAD_STORE);
#endif
}

int uop_dispatch_loop(void)
{
	process_pending_exceptions();

	/* main dispatch loop */
	for(;;) {
		struct uop *op;

		UOP_TRACE(10, "\nUOP: start of new cycle\n");

		// in the last instruction we wrote something else into r[PC], so sync it with
		// the real program counter cpu.pc
		if(unlikely(cpu.r15_dirty)) {
			UOP_TRACE(9, "UOP: r15 dirty\n");
			cpu.r15_dirty = FALSE;

			if(cpu.curr_cp) {
				if((cpu.pc >> MMU_PAGESIZE_SHIFT) == (cpu.r[PC] >> MMU_PAGESIZE_SHIFT)) {
					cpu.cp_pc = PC_TO_CPPC(cpu.r[PC]);
				} else {
					cpu.curr_cp = NULL; // will load a new codepage in a few lines
				}
			}
			cpu.pc = cpu.r[PC];
		}

		// check for exceptions
		if(unlikely(cpu.pending_exceptions != 0)) {
			// something may be pending
			if(cpu.pending_exceptions & ~(cpu.cpsr & (PSR_IRQ_MASK|PSR_FIQ_MASK))) {
				if(process_pending_exceptions())
					continue;
			}
		}

		/* see if we are off the end of a codepage, or the codepage was removed out from underneath us */
		if(unlikely(cpu.curr_cp == NULL)) {
			UOP_TRACE(7, "UOP: curr_cp == NULL, setting new codepage\n");
			if(set_codepage(cpu.pc))
				continue; // MMU translation error reading it
		}

		ASSERT(cpu.curr_cp != NULL);
		ASSERT(cpu.cp_pc != NULL);

		// instruction count
		inc_perf_counter(INS_COUNT);
#if COUNT_CYCLES
		inc_perf_counter(CYCLE_COUNT);
#endif

		/* get the next op and dispatch it */
		op = cpu.cp_pc;
		UOP_TRACE(8, "UOP: opcode %3d %32s, pc 0x%x, cp_pc %p, curr_cp %p\n", op->opcode, uop_opcode_to_str(op->opcode), cpu.pc, cpu.cp_pc, cpu.curr_cp);
#if COUNT_UOPS
		inc_perf_counter(UOP_BASE + op->opcode);
#endif

		/* increment the program counter */
		int pc_inc = cpu.curr_cp->pc_inc;
		cpu.pc += pc_inc; // next pc
		cpu.r[PC] = cpu.pc + pc_inc; // during the course of the instruction, r15 looks like it's +8 or +4 (arm vs thumb)
		cpu.cp_pc++;

		if(TRACE_CPU_LEVEL >= 10 
		   && op->opcode != DECODE_ME_ARM 
		   && op->opcode != DECODE_ME_THUMB)
			dump_cpu();

		/* check to see if we should execute it */
		if(unlikely(!check_condition(op->cond))) {
			UOP_TRACE(8, "UOP: opcode not executed due to condition 0x%x\n", op->cond);
#if COUNT_ARM_OPS
			inc_perf_counter(OP_SKIPPED_CONDITION);
#endif
			continue; // not executed
		}

		/* dispatch */
		switch(op->opcode) {
			case NOP:
#if COUNT_ARM_OPS
				inc_perf_counter(OP_NOP);
#endif
				break;
			case DECODE_ME_ARM:
				uop_decode_me_arm(op);
				break;
			case DECODE_ME_THUMB:
				uop_decode_me_thumb(op);
				break;
			case B_IMMEDIATE: 
				uop_b_immediate(op);
				break;
			case B_IMMEDIATE_LOCAL: 
				uop_b_immediate_local(op);
				break;
			case B_REG:
				uop_b_reg(op);
				break;
			case B_REG_OFFSET:
				uop_b_reg_offset(op);
				break;
			case LOAD_IMMEDIATE_WORD:
				uop_load_immediate_word(op);
				break;
			case LOAD_IMMEDIATE_HALFWORD:
				uop_load_immediate_halfword(op);
				break;
			case LOAD_IMMEDIATE_BYTE: 
				uop_load_immediate_byte(op);
				break;
			case LOAD_IMMEDIATE_OFFSET:
				uop_load_immediate_offset(op);
				break;
			case LOAD_SCALED_REG_OFFSET:
				uop_load_scaled_reg_offset(op);
				break;
			case STORE_IMMEDIATE_OFFSET:
				uop_store_immediate_offset(op);
				break;
			case STORE_SCALED_REG_OFFSET:
				uop_store_scaled_reg_offset(op);
				break;
			case LOAD_MULTIPLE:			// simple multiple load, no S bit
				uop_load_multiple(op);
				break;
			case LOAD_MULTIPLE_S:				// multiple load, with S bit
				uop_load_multiple_s(op);
				break;
			case STORE_MULTIPLE:			// simple multiple store, no S bit
				uop_store_multiple(op);
				break;
			case STORE_MULTIPLE_S:		// multiple store with S bit
				uop_store_multiple_s(op);
				break;
			case DATA_PROCESSING_IMM:		// plain instruction, no barrel shifter, no condition flag update, immediate operand
				uop_data_processing_imm(op);
				break;
			case DATA_PROCESSING_REG:		// plain instruction, no barrel shifter, no condition flag update, no writeback, register operand
				uop_data_processing_reg(op);
				break;
			case DATA_PROCESSING_IMM_S:	// same as above but S bit set, update condition flags, writeback conditional on op
				uop_data_processing_imm_s(op);
				break;
			case DATA_PROCESSING_REG_S:	// same as above but S bit set, update condition flags, writeback conditional on op
				uop_data_processing_reg_s(op);
				break;
			case DATA_PROCESSING_IMM_SHIFT: // barrel shifter involved, immediate operands to shifter, S bit may apply
				uop_data_processing_imm_shift(op);
				break;
			case DATA_PROCESSING_REG_SHIFT:	// barrel shifter involved, register operands to shifter, S bit may apply
				uop_data_processing_reg_shift(op);
				break;
			case MOV_IMM: // move immediate value into register
				uop_mov_imm(op);
				break;
			case MOV_IMM_NZ: // move immediate value into register, set NZ condition
				uop_mov_imm_nz(op);
				break;
			case MOV_REG: // move one register to another
				uop_mov_reg(op);
				break;
			case CMP_IMM_S: // compare with immediate, set full conditions
				uop_cmp_imm_s(op);
				break;
			case CMP_REG_S: // compare with another register, set full conditions
				uop_cmp_reg_s(op);
				break;
			case CMN_REG_S:
				uop_cmn_reg_s(op);
				break;
			case TST_REG_S:
				uop_tst_reg_s(op);
				break;
			case ADD_IMM: // add a value to a register and store it into another register
				uop_add_imm(op);
				break;
			case ADD_IMM_S: // add a value to a register and store it into another register, set condition flags
				uop_add_imm_s(op);
				break;
			case ADD_REG: // add two registers and store in a third
				uop_add_reg(op);
				break;
			case ADD_REG_S: // add two registers and store in a third, set condition flags
				uop_add_reg_s(op);
				break;
			case ADC_REG_S:
				uop_adc_reg_s(op);
				break;
			case SUB_REG_S: // add two registers and store in a third, set condition flags
				uop_sub_reg_s(op);
				break;
			case SBC_REG_S:
				uop_sbc_reg_s(op);
				break;
			case ORR_IMM:
				uop_orr_imm(op);
				break;			
			case ORR_REG_S:
				uop_orr_reg_s(op);
				break;			
			case LSL_IMM:	// logical left shift by immediate
				uop_lsl_imm(op);
				break;
			case LSL_IMM_S:	// logical left shift by immediate, sets full conditions
				uop_lsl_imm_s(op);
				break;
			case LSL_REG:	// logical left shift by register
				uop_lsl_reg(op);
				break;
			case LSL_REG_S:	// logical left shift by register, sets full conditions
				uop_lsl_reg_s(op);
				break;
			case LSR_IMM:	// logical right shift by immediate
				uop_lsr_imm(op);
				break;
			case LSR_IMM_S:	// logical right shift by immediate, sets full conditions
				uop_lsr_imm_s(op);
				break;
			case LSR_REG:	// logical right shift by register
				uop_lsr_reg(op);
				break;
			case LSR_REG_S:	// logical right shift by register, sets full conditions
				uop_lsr_reg_s(op);
				break;
			case ASR_IMM:	// arithmetic right shift by immediate
				uop_asr_imm(op);
				break;
			case ASR_IMM_S:	// arithmetic right shift by immediate, sets full conditions
				uop_asr_imm_s(op);
				break;
			case ASR_REG:	// arithmetic right shift by register
				uop_asr_reg(op);
				break;
			case ASR_REG_S:	// arithmetic right shift by register, sets full conditions
				uop_asr_reg_s(op);
				break;
			case ROR_REG:	// rotate right by register
				uop_ror_reg(op);
				break;
			case ROR_REG_S:	// rotate right by register, sets full conditions
				uop_ror_reg_s(op);
				break;
			case AND_REG_S:
				uop_and_reg_s(op);
				break;			
			case EOR_REG_S:
				uop_eor_reg_s(op);
				break;			
			case BIC_REG_S:
				uop_bic_reg_s(op);
				break;			
			case NEG_REG_S:
				uop_neg_reg_s(op);
				break;			
			case MVN_REG_S:
				uop_mvn_reg_s(op);
				break;			
			case MULTIPLY:
				uop_multiply(op);
				break;
			case MULTIPLY_LONG:
				uop_multiply_long(op);
				break;
		    case COUNT_LEADING_ZEROS:
				uop_count_leading_zeros(op);
				break;
			case MOVE_TO_SR_IMM:
				uop_move_to_sr_imm(op);
				break;
			case MOVE_TO_SR_REG:
				uop_move_to_sr_reg(op);
				break;
			case MOVE_FROM_SR:
				uop_move_from_sr(op);
				break;
			case UNDEFINED:
				uop_undefined(op);
				break;
			case SWI:
				uop_swi(op);
				break;
			case BKPT:
				uop_bkpt(op);
				break;
			case COPROC_REG_TRANSFER:
				uop_coproc_reg_transfer(op);
				break;
			case COPROC_DOUBLE_REG_TRANSFER:
				uop_coproc_double_reg_transfer(op);
				break;
			case COPROC_DATA_PROCESSING:
				uop_coproc_data_processing(op);
				break;
			case COPROC_LOAD_STORE:
				uop_coproc_load_store(op);
				break;
			default:
				panic_cpu("bad uop decode, bailing...\n");
		}
	}

	return 0;
}

