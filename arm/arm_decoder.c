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

// opcode[27:25] == 0b000
static void prim_group_0_decode(struct uop *op)
{
	/* look for special cases (various miscellaneous forms) */
	switch(op->undecoded.raw_instruction & ((3<<23)|(1<<20)|(1<<7)|(1<<4))) {
		default:
			op_data_processing(op); // data processing immediate shift and register shift
			break;
		// cases of bits 24:23 == 2 and bit 20 == 0 and bit 7 == 0
		case (2<<23):
	    case (2<<23)|(1<<4): { // miscellaneous instructions (Figure 3-3, page A3-4). 
			// also section 3.13.3, page A3-30 (control instruction extension space)
			unsigned int op1 = BITS_SHIFT(op->undecoded.raw_instruction, 22, 21);

			// switch based off of bits 7:4
			switch(op->undecoded.raw_instruction & (0x7 << 4)) {
				case (0<<4):
					if(op1 & 1)
						op_msr(op); // msr
					else
						op_mrs(op); // mrs
					break;
				case (1<<4):
					switch(op1) {
						case 1:
							op_bx(op); // bx
							break;
						case 3:
							op_clz(op); // clz
							break;
						default:
							op_undefined(op); // undefined
							break;
					}
					break;
				case (3<<4):
					if(op1 == 1) {
						op_bx(op); // blx, register form
					} else {
						op_undefined(op); // undefined
					}
					break;
				case (5<<4):
					bad_decode(op); // XXX DSP add/subtracts go here (qadd, qsub, qdadd, qdsub)
					break;
				case (7<<4):
					if(op1 == 1) {
						op_bkpt(op); // bkpt
					} else {
						op_undefined(op); // undefined
					}
					break;
				default:
					op_undefined(op);
					break;
			}
			break;
		}
        // cases of bits 24:23 == 2 and bit 20 == 0 and bit 7 == 1 and bit 4 == 0
		case (2<<23)|(1<<7):
			bad_decode(op); // XXX DSP multiplies go here (smla, smlaw, smulw, smlal, smul)
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
						case 0:
							if (BIT(op->undecoded.raw_instruction, 22))
								bad_decode(op); // umaal, armv6
							else
								op_mul(op); // mul/mla
							break;
						case (1<<23):
							op_mull(op); // umull/smull/umlal/smlal, umaal
							break;
						case (2<<23):
							op_swap(op); // swp/swpb
							break;
						case (3<<23):
							bad_decode(op); // armv6 ldrex/strex
							break;
					}
					break;
				case (1<<5): // load/store halfword
					op_load_store_halfword(op);
					break;
				default:	 // load/store halfward and load/store two words
					if(op->undecoded.raw_instruction & (1<<20)) {
						op_load_store_halfword(op); // load store halfword
					} else {
						op_load_store_two_word(op); // load store two words
					}
					break;
			}
			break;
		}			
	}
}

// opcode[27:25] == 0b001
static void prim_group_1_decode(struct uop *op)
{
	/* look for special cases (undefined, move immediate to status reg) */
	switch(BITS_SHIFT(op->undecoded.raw_instruction, 24, 20)) {
		default:
			op_data_processing(op);
			break;
		case 0x12: case 0x16: // MSR, immediate form
			op_msr(op);
			break;
		case 0x10: case 0x14: // undefined
			op_undefined(op);
			break;
	}
}

// opcode[27:25] == 0b011
static void prim_group_3_decode(struct uop *op)
{
	/* look for armv6 media instructions */
	if(op->undecoded.raw_instruction & (1<<4)) {
		switch (BITS_SHIFT(op->undecoded.raw_instruction, 24, 23)) {
			case 0:
				/* parallel add/subtract */
				bad_decode(op);
			case 1:
				/* halfword pack */
				/* word saturate */
				/* parallel halfword saturate */
				/* byte reverse word */
				/* byte reverse packed halfword */
				/* byte reverse signed halfword */
				/* select bytes */
				/* sign/zero extend with add */
				bad_decode(op);
			case 2:
				/* type 3 multiplies */
				bad_decode(op);
			case 3:
				/* unsigned sum of absolute differences */
				/* unsigned sum of absolute differences + accumulator */
				bad_decode(op);
		}
		return;
	}

	op_load_store(op); // general load/store
}

// opcode[27:25] == 0b110
static void prim_group_6_decode(struct uop *op)
{
	/* look for the coprocessor instruction extension space */
	if((BITS(op->undecoded.raw_instruction, 24, 23) | BIT(op->undecoded.raw_instruction, 21)) == 0) {
		if(BIT(op->undecoded.raw_instruction, 22)) {
			op_coproc_double_reg_transfer(op); // mcrr/mrrc, for ARMv5e
		} else {
			op_undefined(op);
		}
	} else {
		op_coproc_load_store(op);
	}
}

// opcode[27:25] == 0b111
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
	op_load_store,       // load/store immediate offset
	prim_group_3_decode, // load/store register offset, armv6 media
	op_load_store_multiple, // load/store multiple 
	op_branch,  // branch op
	prim_group_6_decode, // coprocessor load/store and double reg transfers
	prim_group_7_decode, // coprocessor data processing/register transfers, swi, undefined instructions
};

void arm_decode_into_uop(struct uop *op)
{
	/* look for the unconditional instruction extension space. page A3-34 */
	// XXX are these "always" instructions on V4?
	if (unlikely(((op->undecoded.raw_instruction >> COND_SHIFT) & COND_MASK) == COND_SPECIAL)) {
		unsigned int opcode1 = BITS_SHIFT(op->undecoded.raw_instruction, 27, 20);
		if (opcode1 == 0x10) {
			if (BIT(op->undecoded.raw_instruction, 16)) {
				// XXX SETEND
				bad_decode(op);
			} else {
				// XXX CPS
				bad_decode(op);
			}
		} else if((opcode1 & 0xd7) == 0x55) {
			op_pld(op); // pld
		} else if((opcode1 & 0xe5) == 0x84) {
			// XXX SRS
			bad_decode(op);
		} else if((opcode1 & 0xe5) == 0x81) {
			// XXX RFE
			bad_decode(op);
		} else if ((opcode1 & 0xe0) == 0xa0) {
			op_branch(op); // blx (address form)
		} else if((opcode1 & 0xe0) == 0xc0) {
			// XXX stc2/ldc2
			bad_decode(op);
		} else if((opcode1 & 0xf0) == 0xe0) {
			// XXX cdp2/mcr2/mrc2
			bad_decode(op);
		} else if((opcode1 & 0xf0) == 0xf0) {
			// genuine undefined instructions
			op_undefined(op);
		} else {
			// something slipped through the cracks
			bad_decode(op);
		}
	} else {
		ins_group_table[BITS_SHIFT(op->undecoded.raw_instruction, 27, 25)](op);
	}
}


