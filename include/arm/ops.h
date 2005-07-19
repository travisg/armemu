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
void op_load_store_halfword(struct uop *op);
void op_load_store_two_word(struct uop *op);
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
