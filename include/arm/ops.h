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
#ifndef __ARM_OPS_H
#define __ARM_OPS_H

#include <arm/arm.h>

/* ARM ops */
void op_branch(struct uop *op);
void op_bx(struct uop *op);
void op_msr(struct uop *op);
void op_mrs(struct uop *op);
void op_undefined(struct uop *op);
void op_data_processing(struct uop *op);
void op_mul(struct uop *op);
void op_mull(struct uop *op);
void op_swap(struct uop *op);
void op_load_store(struct uop *op);
void op_load_store_multiple(struct uop *op);
void op_misc_load_store(struct uop *op);
void op_swi(struct uop *op);
void op_bkpt(struct uop *op);
void op_clz(struct uop *op);
void op_pld(struct uop *op);
void op_coproc_reg_transfer(struct uop *op);
void op_coproc_double_reg_transfer(struct uop *op);
void op_coproc_data_processing(struct uop *op);
void op_coproc_load_store(struct uop *op);
void op_no_coproc_installed(struct uop *op, int cp_num);

/* data processing opcode to string */
const char *dp_op_to_str(int op);

/* thumb ops */
void thumb_op_shift_imm(struct uop *op);
void thumb_op_add_sub_3reg(struct uop *op);
void thumb_op_add_sub_large_immediate(struct uop *op);
void thumb_op_data_processing(struct uop *op);
void thumb_op_special_data_processing(struct uop *op);
void thumb_op_bx_reg(struct uop *op);
void thumb_op_literal_load(struct uop *op);
void thump_op_load_store_register(struct uop *op);
void thumb_op_load_store_immediate_offset(struct uop *op);
void thumb_op_load_store_from_stack(struct uop *op);
void thumb_op_add_to_sp_pc(struct uop *op);
void thumb_op_adjust_sp(struct uop *op);
void thumb_op_push_pop_regs(struct uop *op);
void thumb_op_software_breakpoint(struct uop *op);
void thumb_op_undefined(struct uop *op);
void thumb_op_load_store_multiple(struct uop *op);
void thumb_op_conditional_branch(struct uop *op);
void thumb_op_swi(struct uop *op);
void thumb_op_branch(struct uop *op);
void thumb_op_bl_blx(struct uop *op);
void thumb_op_blx_suffix(struct uop *op);
void thumb_op_bl_suffix(struct uop *op);

#endif
