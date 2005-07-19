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

void thumb_decode_into_uop(struct uop *op)
{
	halfword ins = op->undecoded.raw_instruction;

	// switch based off the top 5 bits of the instruction
	switch(BITS_SHIFT(ins, 15, 11)) {
		case 0x00:
		case 0x01:
		case 0x02:
			// shift by immediate
			thumb_op_shift_imm(op);
			break;
		case 0x03:
			// add/subtract 3 register
			thumb_op_add_sub_3reg(op);
			break;
		case 0x04:
		case 0x05:
		case 0x06:
		case 0x07:
			// add/subtract/compare/move immediate
			thumb_op_add_sub_large_immediate(op);
			break;
		case 0x08:
			if(BIT(ins, 10)) {
				if(BITS_SHIFT(ins, 9, 8) == 3) {
					// branch/exchange instruction set (also BLX)
					thumb_op_bx_reg(op);
				} else {
					// special data processing
					thumb_op_special_data_processing(op);
				}
			} else {
				// data-processing register
				thumb_op_data_processing(op);
			}
			break;
		case 0x09:
			// load from literal pool
			thumb_op_literal_load(op);
			break;
		case 0x0a:
		case 0x0b:
			// load/store register offset
			thump_op_load_store_register(op);
			break;
		case 0x0c:
		case 0x0d:
		case 0x0e:
		case 0x0f:
			// load/store word/byte immediate offset
			thumb_op_load_store_immediate_offset(op);
			break;
		case 0x10:
		case 0x11:
			// load/store halfword immediate offset
			thumb_op_load_store_immediate_offset(op);
			break;
		case 0x12:
		case 0x13:
			// load/store to/from stack
			thumb_op_load_store_from_stack(op);
			break;
		case 0x14:
		case 0x15:
			// add to sp or pc
			thumb_op_add_to_sp_pc(op);
			break;
		case 0x16:
		case 0x17:
			// miscellaneous (figure 6-2, page A6-5)
			switch(BITS_SHIFT(ins, 11, 8)) {
				case 0:
					// adjust stack pointer
					thumb_op_adjust_sp(op);
					break;
				case 0x4:
				case 0x5:
				case 0xc:
				case 0xd:
					// push/pop register list
					thumb_op_push_pop_regs(op);
					break;
				case 0xe:
					// software breakpoint
					thumb_op_software_breakpoint(op);
					break;
				default:
					// undefined instruction
					thumb_op_undefined(op);
					break;	
			}
			break;
		case 0x18:
		case 0x19:
			// load store multiple
			thumb_op_load_store_multiple(op);
			break;
		case 0x1a:
		case 0x1b:
			switch(BITS_SHIFT(ins, 11, 8)) {
				default:
					// conditional branch
					thumb_op_conditional_branch(op);
					break;
				case 0xe:
					// undefined instruction
					thumb_op_undefined(op);
					break;
				case 0xf:
					// software interrupt
					thumb_op_swi(op);
					break;
			}
			break;
		case 0x1c:
			// unconditional branch
			thumb_op_branch(op);
			break;
		case 0x1d:
			if(ins & 1) {
				// undefined instruction
				thumb_op_undefined(op);
			} else {
				// blx suffix
				thumb_op_bl_blx(op);
			}		
			break;
		case 0x1e:
		case 0x1f:
			// bl/blx prefix
			thumb_op_bl_blx(op);
			break;
	}
}
