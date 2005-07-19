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

#include <arm/arm.h>
#include <arm/decoder.h>
#include <arm/ops.h>

typedef void (*decode_stage_func)(struct uop *op);

static void bad_decode(struct uop *op)
{
	panic_cpu("bad_decode: ins 0x%08x at 0x%08x\n", op->undecoded.raw_instruction, cpu.pc - 4);
}

static void prim_group_0_decode(struct uop *op)
{
	/* undefined */
	if(((op->undecoded.raw_instruction >> COND_SHIFT) & COND_MASK) == COND_SPECIAL) {
		op_undefined(op);
		return;
	}

	/* look for special cases (various miscellaneous forms) */
	switch(op->undecoded.raw_instruction & ((3<<23)|(1<<20)|(1<<7)|(1<<4))) {
		default:
			op_data_processing(op); // data processing immediate shift and register shift
			break;
		// cases of bit 20 and 7 not set
		case (2<<23):
		case (2<<23)|(1<<4): // miscellaneous instructions (Figure 3-3, page A3-4)
			// switch based off of bits 7:4
			switch(op->undecoded.raw_instruction & (0xf << 4)) {
				case (0<<4): // msr/mrs
					if(op->undecoded.raw_instruction & (1<<21))
						op_msr(op);
					else
						op_mrs(op);
					break;
				case (1<<4): // bx, clz
					switch(op->undecoded.raw_instruction & (3<<21)) {
						case (1<<21): // bx
							op_bx(op);
							break;
						case (3<<21): // clz
							op_clz(op);
							break;
						default:
							bad_decode(op); // undefined
							break;
					}
					break;
				case (3<<4):
					if((op->undecoded.raw_instruction & (3<<21)) == (1<<21)) {
						op_bx(op); // blx
					} else {
						bad_decode(op); // undefined
					}
					break;
				case (5<<4):
					bad_decode(op); // XXX DSP add/subtracts go here
					break;
				case (7<<4):
					if((op->undecoded.raw_instruction & (3<<21)) == (1<<21)) {
						op_bkpt(op);
					} else {
						bad_decode(op); // undefined
					}
					break;
				default:
					bad_decode(op);
					break;				
			}
			break;
		// cases of bit 7 and 4 not set
		case (2<<23)|(1<<7):
			bad_decode(op); // XXX dsp multiplies go here
			break;
		// all cases of bit 7 and 4 being set
		case                 (1<<7)|(1<<4):
		case (1<<23)|        (1<<7)|(1<<4):
		case (2<<23)|        (1<<7)|(1<<4):
		case (3<<23)|        (1<<7)|(1<<4):
		case         (1<<20)|(1<<7)|(1<<4):
		case (1<<23)|(1<<20)|(1<<7)|(1<<4):
		case (2<<23)|(1<<20)|(1<<7)|(1<<4):
		case (3<<23)|(1<<20)|(1<<7)|(1<<4):	{ // multiplies, extra load/stores (Figure 3-2, page A3-3)
			switch(op->undecoded.raw_instruction & (3<<5)) {
				case 0: // multiply, multiply long, swap
					switch(op->undecoded.raw_instruction & (3<<23)) {
						case 0: // multiply
							op_mul(op);
							break;
						case (1<<23): // multiply long
							op_mull(op);
							break;
						case (2<<23): // swap
							op_swap(op);
							break;
						default:
							op_undefined(op);
					}
					break;
				case (1<<5): // load/store halfword
					op_load_store_halfword(op);
					break;
				default:	 // load/store halfward and load/store two words
					if(op->undecoded.raw_instruction & (1<<20)) {
						// load store halfword
						op_load_store_halfword(op);
					} else {
						// load store two words
						op_load_store_two_word(op);
					}
					break;
			}
			break;
		}			
	}
}

static void prim_group_1_decode(struct uop *op)
{
	/* undefined */
	if(((op->undecoded.raw_instruction >> COND_SHIFT) & COND_MASK) == COND_SPECIAL) {
		op_undefined(op);
		return;
	}

	/* look for special cases (undefined, move immediate to status reg) */
	switch(op->undecoded.raw_instruction & (0x1f << 20)) {
		default:
			op_data_processing(op);
			break;
		case 0x12<<20: case 0x16<<20: // MSR, immediate
			op_msr(op);
			break;
		case 0x10<<20: case 0x14<<20: // undefined
			op_undefined(op);
			break;
	}
}

static void prim_group_2_decode(struct uop *op)
{
	/* undefined */
	if(((op->undecoded.raw_instruction >> COND_SHIFT) & COND_MASK) == COND_SPECIAL) {
		/* look for PLD instruction */
		if((BITS_SHIFT(op->undecoded.raw_instruction, 24, 20) & 0x17) == 0x15) {
			op_pld(op);
		} else {
			op_undefined(op);
		}
		return;
	}

	op_load_store(op);
}

static void prim_group_3_decode(struct uop *op)
{
	/* unconditional instruction space */
	if(((op->undecoded.raw_instruction >> COND_SHIFT) & COND_MASK) == COND_SPECIAL) {
		/* look for PLD instruction */
		if((BITS_SHIFT(op->undecoded.raw_instruction, 24, 20) & 0x17) == 0x15) {
			op_pld(op);
		} else {
			op_undefined(op);
		}
		return;
	}

	/* look for a particular undefined form */
	if(op->undecoded.raw_instruction & (1<<4)) {
		op_undefined(op);
		return;
	}

	op_load_store(op);
}

static void prim_group_4_decode(struct uop *op)
{
	/* undefined */
	if(((op->undecoded.raw_instruction >> COND_SHIFT) & COND_MASK) == COND_SPECIAL) {
		op_undefined(op);
		return;
	}

	op_load_store_multiple(op);	
}

static void prim_group_6_decode(struct uop *op)
{
	/* look for the coprocessor instruction extension space */
	if((BITS(op->undecoded.raw_instruction, 24, 23) | BIT(op->undecoded.raw_instruction, 21)) == 0) {
		op_coproc_double_reg_transfer(op);
	} else {
		op_coproc_load_store(op);
	}
}

static void prim_group_7_decode(struct uop *op)
{
	/* undefined */
	if(((op->undecoded.raw_instruction >> COND_SHIFT) & COND_MASK) == COND_SPECIAL) {
		op_undefined(op);
		return;
	}

	/* switch between swi and coprocessor */
	if(BIT(op->undecoded.raw_instruction, 24)) {
		op_swi(op);
	} else {
		if(BIT(op->undecoded.raw_instruction, 4)) {
			op_coproc_reg_transfer(op);
		} else {
			op_coproc_data_processing(op);
		}
	}
}

const decode_stage_func ins_group_table[] = {
	prim_group_0_decode, // data processing, multiply, load/store halfword/two words
	prim_group_1_decode, // data processing immediate and move immediate to status reg
	prim_group_2_decode, // load/store immediate offset
	prim_group_3_decode, // load/store register offset
	prim_group_4_decode, // load/store multiple
	op_branch,  // branch op
	prim_group_6_decode, // coprocessor load/store and double reg transfers
	prim_group_7_decode, // coprocessor data processing/register transfers, swi, undefined instructions
};

void arm_decode_into_uop(struct uop *op)
{
	ins_group_table[BITS_SHIFT(op->undecoded.raw_instruction, 27, 25)](op);
}


