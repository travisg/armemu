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
#ifndef __ARM_H
#define __ARM_H

#include "debug.h"
#include "systypes.h"

typedef word reg_t;
typedef word armaddr_t;

#include <arm/mmu.h>
#include <arm/uops.h>

// used in ASSERT() so declared up front
void panic_cpu(const char *fmt, ...);
void shutdown_cpu(void);

enum arm_instruction_set {
	ARM_V4 = 0,
	ARM_V5,
	ARM_V5e,
	ARM_V6
};

enum arm_core {
	ARM7 = 0,
	ARM9,
	ARM9e,
};

/* performance counters, including current cycle and instruction count */
/* to disable for speed purposes, move MAX_PERF_COUNTER up in the enum */
enum perf_counter_type {
	INS_COUNT,

	EXCEPTIONS,

	INS_DECODE,

#if COUNT_MMU_OPS
	MMU_READ,
	MMU_WRITE,
	MMU_INS_FETCH,
	MMU_FASTPATH,
	MMU_SLOWPATH,
	MMU_SLOW_TRANSLATE,
#endif

#if COUNT_CYCLES
	CYCLE_COUNT,
#endif
#if COUNT_ARM_OPS
	OP_SKIPPED_CONDITION,
	OP_NOP,
	OP_LOAD,
	OP_STORE,
	OP_DATA_PROC,
	OP_MUL,
	OP_BRANCH,
	OP_COP_REG_TRANS,
	OP_COP_DATA_PROC,
	OP_COP_LOAD_STORE,
	OP_MISC,
#endif

#if COUNT_UOPS
	UOP_BASE,
	UOP_TOP = UOP_BASE + MAX_UOP_OPCODE,
#endif

#if COUNT_ARITH_UOPS
	UOP_ARITH_OPCODE,
	UOP_ARITH_OPCODE_TOP = UOP_ARITH_OPCODE + 16,
#endif

	// setting this lower will tend to make the compiler optimize away calls to add_to_perf_count
	MAX_PERF_COUNTER, 
};

struct perf_counters {
	int count[MAX_PERF_COUNTER];
};

struct arm_coprocessor {
	int installed;
	void *data;

	// the callback routines
	void (*reg_transfer)(word ins, void *data);
	void (*double_reg_transfer)(word ins, void *data);
	void (*data_processing)(word ins, void *data);
	void (*load_store)(word ins, void *data);	
};

struct cpu_struct {
	// regs
	reg_t pc;			// the real pc
	struct uop *cp_pc;	// pointer to the current op
	reg_t cpsr;
	reg_t r[16];		// contains a "fake" pc made to look as if it's ahead of the current pc
	reg_t spsr;
	bool r15_dirty;		// if we wrote into r[15] in the last instruction

	// pending interrupts and mode changes
	volatile int pending_exceptions;
	reg_t old_cpsr; // in case of a mode switch, we store the old mode

	// cache of uop codepages
	struct uop_codepage *curr_cp;
	struct uop_codepage *codepage_hash[CODEPAGE_HASHSIZE];

	// free list of codepage structures
	struct uop_codepage *free_cp_arm;
	struct uop_codepage *free_cp_thumb;

	// truth table of the arm conditions
	unsigned short condition_table[16];

	// cpu config
	enum arm_instruction_set isa;
	enum arm_core core;

	// tracks emulator performance including cycle count and instruction count
	struct perf_counters perf_counters;

	// routines for the arm's 16 coprocessor slots
	struct arm_coprocessor coproc[16];

	// banked_regs
	reg_t usr_regs_low[5]; // non-fiq r8-r12
	reg_t usr_regs[2];     // sp, lr
	reg_t irq_regs[3];     // sp, lr, spsr
	reg_t svc_regs[3];     //     "
	reg_t abt_regs[3];     //     "
	reg_t und_regs[3];     //     "
	reg_t fiq_regs[8];     // r8-r12, sp, lr, spsr
};

extern struct cpu_struct cpu;

// special registers
#define PC 15
#define LR 14
#define SP 13

// CPSR bits
#define PSR_MODE_MASK 0x1f
#define PSR_MODE_user 0x10
#define PSR_MODE_fiq  0x11
#define PSR_MODE_irq  0x12
#define PSR_MODE_svc  0x13
#define PSR_MODE_abt  0x17
#define PSR_MODE_und  0x1b
#define PSR_MODE_sys  0x1f

#define PSR_IRQ_MASK  0x80
#define PSR_FIQ_MASK  0x40
#define PSR_THUMB     0x20

#define PSR_CC_NEG    0x80000000
#define PSR_CC_ZERO   0x40000000
#define PSR_CC_CARRY  0x20000000
#define PSR_CC_OVL    0x10000000
#define PSR_CC_Q      0x08000000

// conditions
#define COND_MASK     0xf
#define COND_EQ       0x0
#define COND_NE       0x1
#define COND_CS       0x2
#define COND_CC       0x3
#define COND_MI       0x4
#define COND_PL       0x5
#define COND_VS       0x6
#define COND_VC       0x7
#define COND_HI       0x8
#define COND_LS       0x9
#define COND_GE       0xa
#define COND_LT       0xb
#define COND_GT       0xc
#define COND_LE       0xd
#define COND_AL       0xe
#define COND_SPECIAL  0xf
#define COND_SHIFT    28

/* pending exception bits */
#define EX_RESET      0x01
#define EX_UNDEFINED  0x02
#define EX_SWI        0x04
#define EX_PREFETCH   0x08
#define EX_DATA_ABT   0x10
#define EX_FIQ        0x40		/* same bits as the cpsr mask bits */
#define EX_IRQ        0x80

/* arm arithmetic opcodes */
enum {
	AOP_AND = 0,
	AOP_EOR,
	AOP_SUB,
	AOP_RSB,
	AOP_ADD,
	AOP_ADC,
	AOP_SBC,
	AOP_RSC,
	AOP_TST,
	AOP_TEQ,
	AOP_CMP,
	AOP_CMN,
	AOP_ORR,
	AOP_MOV,
	AOP_BIC,
	AOP_MVN,
};

/* shift and rotate helpers */
#define LSL(val, shift) \
	((val) << (shift))
#define LSR(val, shift) \
	((val) >> (shift))
#define ASR(val, shift) \
	(((int)(val)) >> (shift))
#define ROR(val, shift) \
	(((val) >> (shift)) | ((val) << (32 - (shift))))


/* bit manipulation macros */
#define BIT(x, bit) ((x) & (1 << (bit)))
#define BIT_SHIFT(x, bit) (((x) >> (bit)) & 1)
#define BITS(x, high, low) ((x) & (((1<<((high)+1))-1) & ~((1<<(low))-1)))
#define BITS_SHIFT(x, high, low) (((x) >> (low)) & ((1<<((high)-(low)+1))-1))
#define BIT_SET(x, bit) (((x) & (1 << (bit))) ? 1 : 0)

#define ISNEG(x) BIT((x), 31)
#define ISPOS(x) (!(BIT(x, 31)))

/* 32-bit sign extension */
//#define SIGN_EXTEND(val, topbit) (BIT(val, topbit) ? ((val) | (0xffffffff << (topbit))) : (val)) 
#define SIGN_EXTEND(val, topbit) (ASR(LSL(val, 32-(topbit)), 32-(topbit)))

/* ARM routines */
reg_t get_reg(int num);
void put_reg(int num, reg_t data);
void put_reg_nopc(int num, reg_t data);
reg_t get_reg_user(int num);            /* "user" mode reg access */
void put_reg_user(int num, reg_t data); /* same */
void set_cpu_mode(int mode);
word do_add(word a, word b, int carry_in, int *carry, int *ovl);
unsigned int get_condition(unsigned int condition);
void set_condition(unsigned int condition, bool set);
void set_NZ_condition(reg_t val);
bool check_condition(byte condition);
bool arm_in_priviledged(void);
enum arm_instruction_set get_isa(void);
enum arm_core get_core(void);
void add_to_perf_counter(enum perf_counter_type counter, int add);
void inc_perf_counter(enum perf_counter_type counter);
#if COUNT_CYCLES
int get_cycle_count(void);
#endif
int get_instruction_count(void);

/* inline overrides of a lot of the above routines */
extern inline word do_add(word a, word b, int carry_in, int *carry, int *ovl)
{
	word val;

#if 1
	val = a + b + carry_in;

	*carry = (ISNEG(a & b) ||				// both operands are negative, or
			 (ISNEG(a ^ b) && ISPOS(val))); // exactly one of the operands is negative, and result is positive
#else
	/* 64 bit version of the add routine to optimize for the carry flag test */
	dword bigval;

	bigval = (dword)a + (dword)b + (dword)carry_in;
	*carry = BIT_SHIFT(bigval, 32);
	val = (word)bigval;
#endif

	*ovl = (!(ISNEG(a ^ b))) && (ISNEG(a ^ val));

//	CPU_TRACE(10, "do_add: a 0x%x b 0x%x carry_in 0x%x, carry %d, ovl %d\n",
//		a, b, carry_in, *carry, *ovl);

#if 0
#if __i386__ || __I386__
	asm("setc (%0)" :: "r"(carry));
	asm("seto (%0)" :: "r"(ovl));
#endif
#endif

	return val;
}

extern inline void set_condition(unsigned int condition, bool set)
{
	if(condition == PSR_THUMB) {
		CPU_TRACE(7, "setting THUMB bit to %d\n", set);
	}

	if(set)
		cpu.cpsr |= condition;
	else
		cpu.cpsr &= ~condition;
}

extern inline void set_NZ_condition(reg_t val)
{
	set_condition(PSR_CC_NEG, BIT(val, 31));
	set_condition(PSR_CC_ZERO, val == 0);
}

extern inline unsigned int get_condition(unsigned int condition)
{
	return (cpu.cpsr & condition);
}

extern inline reg_t get_reg(int num)
{
	ASSERT(num >= 0 && num < 16);
	return cpu.r[num];
}

extern inline void put_reg(int num, reg_t data)
{
	ASSERT(num >= 0 && num < 16);
	cpu.r[num] = data;
	if(num == PC) {
		cpu.r15_dirty = TRUE; // on the next loop, resync the "real" pc (cpu.pc) with cpu.r[15]
		cpu.r[PC] &= ~1;
	}
}

extern inline void put_reg_nopc(int num, reg_t data)
{
	ASSERT(num >= 0 && num < 15);
	cpu.r[num] = data;
}

extern inline bool check_condition(byte condition)
{
	// this happens far more often than not
	if(likely(condition == COND_AL))
		return TRUE;
	// check the instructions condition mask against precomputed values of cpsr		
	return cpu.condition_table[cpu.cpsr >> COND_SHIFT] & (1 << (condition));
}

extern inline bool arm_in_priviledged(void)
{
	return ((cpu.cpsr & PSR_MODE_MASK) != PSR_MODE_user);
}

extern inline enum arm_instruction_set get_isa(void)
{
	return cpu.isa;
}

extern inline enum arm_core get_core(void)
{
	return cpu.core;
}

extern inline void add_to_perf_counter(enum perf_counter_type counter, int add)
{
	if(counter < MAX_PERF_COUNTER)
		cpu.perf_counters.count[counter] += add;
}

extern inline void inc_perf_counter(enum perf_counter_type counter)
{
	add_to_perf_counter(counter, 1);
}

#if COUNT_CYCLES
extern inline int get_cycle_count(void)
{
	return cpu.perf_counters.count[CYCLE_COUNT];
}
#endif

extern inline int get_instruction_count(void)
{
	return cpu.perf_counters.count[INS_COUNT];
}

/* function prototypes */
int initialize_cpu(const char *cpu_type);
void reset_cpu(void);
int start_cpu(void);
void dump_cpu(void);
void dump_registers(void);
int build_condition_table(void);
void install_coprocessor(int cp_num, struct arm_coprocessor *coproc);

/* coprocessor 15 is for system mode stuff */
void install_cp15(void);

/* exceptions */
void raise_irq(void);
void lower_irq(void);
void raise_fiq(void);
void lower_fiq(void);
void signal_data_abort(armaddr_t addr);
void signal_prefetch_abort(armaddr_t addr);
int process_pending_exceptions(void);

/* codepage maintenance */
void flush_all_codepages(void); /* throw away all cached instructions */

#endif
