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
#include <arm/mmu.h>
#include <arm/ops.h>
#include <util/atomic.h>

const char *dp_op_to_str(int op)
{
    switch (op) {
        case AOP_AND:
            return "AND";
        case AOP_EOR:
            return "EOR";
        case AOP_SUB:
            return "SUB";
        case AOP_RSB:
            return "RSB";
        case AOP_ADD:
            return "ADD";
        case AOP_ADC:
            return "ADC";
        case AOP_SBC:
            return "SBC";
        case AOP_RSC:
            return "RSC";
        case AOP_TST:
            return "TST";
        case AOP_TEQ:
            return "TEQ";
        case AOP_CMP:
            return "CMP";
        case AOP_CMN:
            return "CMN";
        case AOP_ORR:
            return "ORR";
        case AOP_MOV:
            return "MOV";
        case AOP_BIC:
            return "BIC";
        case AOP_MVN:
            return "MVN";
        default:
            return "UNK";
    }
}

void op_nop(struct uop *op)
{
    op->opcode = NOP;

    CPU_TRACE(5, "\t\tNOP\n");
}

void op_branch(struct uop *op)
{
    word ins = op->undecoded.raw_instruction;
    int L;
    int exchange;
    armaddr_t offset;
    reg_t pc;

    // decode the instruction
    offset = BITS(ins, 23, 0);
    L = BIT(ins, 24);
    exchange = ((ins >> COND_SHIFT) & COND_MASK) == COND_SPECIAL;

    offset = SIGN_EXTEND(offset, 23);
    offset *= 4;

    if (exchange) {
        if (get_isa() < ARM_V5) {
            // blx is invalid on ARMv4
            op_undefined(op);
            return;
        }

        // this is actually blx. reinterpret the L bit to mean H
        offset |= (L) ? 0x2 : 0;
        L = 1; // force a link
    }

    pc = get_reg(PC);
    if (L) {
        op->flags |= UOPBFLAGS_LINK;
        op->b_immediate.link_target = pc - 4;
    }
    op->b_immediate.target = (pc + offset) & 0xfffffffe;

    // this translates to branch immediate
    if ((op->b_immediate.target >> MMU_PAGESIZE_SHIFT) == ((pc - 8) >> MMU_PAGESIZE_SHIFT))
        op->opcode = B_IMMEDIATE_LOCAL; // it's within the current codepage
    else
        op->opcode = B_IMMEDIATE; // outside the current codepage
    op->cond = (ins >> COND_SHIFT) & COND_MASK;
    if (op->cond == COND_SPECIAL)
        op->cond = COND_AL;
    op->b_immediate.target_cp = NULL;

    CPU_TRACE(5, "\t\tbranch: L %d x %d offset %d pc 0x%x target 0x%x link target 0x%x\n",
              L?1:0, exchange?1:0, offset, pc, op->b_immediate.target, op->b_immediate.link_target);

    if (exchange) {
        op->opcode = B_IMMEDIATE; // B_IMMEDIATE_LOCAL is not allowed, since switching
        // modes changes you to a new codepage
        op->flags |= UOPBFLAGS_SETTHUMB_ALWAYS;
    }
}

void op_bx(struct uop *op)
{
    word ins = op->undecoded.raw_instruction;
    int Rm;

    // decode the instruction
    Rm = BITS(ins, 3, 0);
#define L BIT(ins, 5) /* for blx */

    if (L && get_isa() < ARM_V5) {
        // BLX is undefined on ARMv4
        op_undefined(op);
        return;
    }

    // this is going to translate to branch register
    op->opcode = B_REG;
    op->cond = (ins >> COND_SHIFT) & COND_MASK;
    op->flags |= UOPBFLAGS_SETTHUMB_COND;
    op->b_reg.reg = Rm;
    op->b_reg.link_offset = -4;

    if (L)
        op->flags |= UOPBFLAGS_LINK;

    CPU_TRACE(5, "\t\tbx: L %d, Rm %d\n", L?1:0, Rm);
#undef L
}

void op_msr(struct uop *op)
{
    word ins = op->undecoded.raw_instruction;
    int field_mask;
    word field_mask_bits;

    // start decoding the instruction
    field_mask = BITS_SHIFT(ins, 19, 16);
#define R BIT(ins, 22)

    // calculate the field mask
    if (field_mask == 0xf) {
        // common case
        field_mask_bits = 0xffffffff;
    } else {
        field_mask_bits = 0;
        if (field_mask & 1)
            field_mask_bits |= 0xff;
        if (field_mask & 2)
            field_mask_bits |= 0xff00;
        if (field_mask & 4)
            field_mask_bits |= 0xff0000;
        if (field_mask & 8)
            field_mask_bits |= 0xff000000;
    }

    // figure out the form
    if (BIT(ins, 25)) {
        // IMMEDIATE OPERAND
        reg_t immediate = BITS(ins, 7, 0);
        int rotate_imm = BITS_SHIFT(ins, 11, 8);
        reg_t operand;

        rotate_imm *= 2;
        operand = ROR(immediate, rotate_imm);

        op->opcode = MOVE_TO_SR_IMM;
        op->cond = (ins >> COND_SHIFT) & COND_MASK;
        if (R)
            op->flags |= UOPMSR_R_BIT;
        op->move_to_sr_imm.immediate = operand;
        op->move_to_sr_imm.field_mask = field_mask_bits;

        CPU_TRACE(5, "\t\top_msr: IMM R %d immediate 0x%x mask 0x%x\n", R?1:0, operand, field_mask_bits);
    } else {
        int Rm = BITS(ins, 3, 0);

        op->opcode = MOVE_TO_SR_REG;
        op->cond = (ins >> COND_SHIFT) & COND_MASK;
        if (R)
            op->flags |= UOPMSR_R_BIT;
        op->move_to_sr_reg.reg = Rm;
        op->move_to_sr_reg.field_mask = field_mask_bits;

        CPU_TRACE(5, "\t\top_msr: REG R %d reg %d mask 0x%x\n", R?1:0, Rm, field_mask_bits);
    }
#undef R
}

void op_mrs(struct uop *op)
{
    word ins = op->undecoded.raw_instruction;
    int Rd;

    // start decoding the instruction
    Rd = BITS_SHIFT(ins, 15, 12);
#define R BIT(ins, 22)

    // translate
    op->opcode = MOVE_FROM_SR;
    op->cond = (ins >> COND_SHIFT) & COND_MASK;
    if (R)
        op->flags |= UOPMSR_R_BIT;
    op->move_from_sr.reg = Rd;

    CPU_TRACE(5, "\t\top_mrs: R %d, Rd %d\n", R?1:0, Rd);
#undef R
}

void op_clz(struct uop *op)
{
    word ins = op->undecoded.raw_instruction;
    int Rd, Rm;

    // only supported on ARMv5+
    if (get_isa() < ARM_V5) {
        op_undefined(op);
        return;
    }

    // decode the instruction
    Rd = BITS_SHIFT(ins, 15, 12);
    Rm = BITS(ins, 3, 0);

    // translate
    op->opcode = COUNT_LEADING_ZEROS;
    op->cond = (ins >> COND_SHIFT) & COND_MASK;
    op->count_leading_zeros.dest_reg = Rd;
    op->count_leading_zeros.source_reg = Rm;

    CPU_TRACE(5, "\t\top_clz: Rd %d Rm %d\n", Rd, Rm);
}

void op_pld(struct uop *op)
{
    word ins = op->undecoded.raw_instruction;
    int Rd;

    // only supported on ARMv5e
    if (get_isa() < ARM_V5e) {
        op_undefined(op);
        return;
    }

    Rd = BITS_SHIFT(ins, 19, 16);

    op->opcode = NOP;
    op->cond = COND_AL;

    CPU_TRACE(5, "\t\top_pld: R %d, treating as NOP\n", Rd);
}

void op_bkpt(struct uop *op)
{
    word ins = op->undecoded.raw_instruction;

    // only supported on ARMv5+
    if (get_isa() < ARM_V5) {
        op_undefined(op);
        return;
    }

    op->opcode = BKPT;
    op->cond = COND_AL;

    CPU_TRACE(5, "bkpt 0x%x\n", ins & 0x00ffffff);
}

void op_swi(struct uop *op)
{
    word ins = op->undecoded.raw_instruction;

    op->opcode = SWI;
    op->cond = (ins >> COND_SHIFT) & COND_MASK;

    CPU_TRACE(5, "swi 0x%x\n", ins & 0x00ffffff);
}

void op_cps(struct uop *op)
{
    word ins = op->undecoded.raw_instruction;

    if (get_isa() < ARM_V6) {
        op_undefined(op);
        return;
    }

    int imod = BITS_SHIFT(ins, 19, 18);
    int mode = BITS(ins, 4, 0);
#define M BIT(ins, 17)
#define A BIT(ins, 8)
#define I BIT(ins, 7)
#define F BIT(ins, 6)

    CPU_TRACE(5, "cps imod 0x%x mode 0x%x setmode %d A %d I %d F %d\n",
              imod, mode, M ? 1 : 0, A ? 1 : 0, I ? 1 : 0, F ? 1 : 0);

    op->opcode = CPS;
    op->flags = 0;
    op->cond = COND_AL;
    op->cps.aif_set = 0;
    op->cps.aif_clear = 0;
    op->cps.mode = mode;

    op->flags |= M ? UOPCPS_SETMODE : 0;

    if (imod == 0x2) {
        // bits to be set in the AIF fields in the CPSR
        op->cps.aif_set = A | I | F;
    } else if (imod == 0x3) {
        // bits to be cleared in the AIF fields in the CPSR
        op->cps.aif_clear = A | I | F;
    }

#undef F
#undef I
#undef A
#undef M
}

void op_undefined(struct uop *op)
{
    word ins = op->undecoded.raw_instruction;

    op->opcode = UNDEFINED;
    op->cond = (ins >> COND_SHIFT) & COND_MASK;
    if (op->cond == COND_SPECIAL)
        op->cond = COND_AL;

    CPU_TRACE(5, "undefined arm instruction 0x%x\n", ins);
}

void op_coproc_reg_transfer(struct uop *op)
{
    word ins = op->undecoded.raw_instruction;
    int cp_num = BITS_SHIFT(ins, 11, 8);

    op->opcode = COPROC_REG_TRANSFER;
    op->cond = (ins >> COND_SHIFT) & COND_MASK;
    op->coproc.raw_instruction = ins;
    op->coproc.cp_num = cp_num;

    CPU_TRACE(5, "\t\tcoproc: num %d REG_TRANS ins 0x%x\n", cp_num, ins);
}

void op_coproc_double_reg_transfer(struct uop *op)
{
    word ins = op->undecoded.raw_instruction;
    int cp_num = BITS_SHIFT(ins, 11, 8);

    // only supported on ARMv5+
    if (get_isa() < ARM_V5) {
        op_undefined(op);
        return;
    }

    op->opcode = COPROC_DOUBLE_REG_TRANSFER;
    op->cond = (ins >> COND_SHIFT) & COND_MASK;
    op->coproc.raw_instruction = ins;
    op->coproc.cp_num = cp_num;

    CPU_TRACE(5, "\t\tcoproc: num %d DOUBLE_REG_TRANS ins 0x%x\n", cp_num, ins);
}

void op_coproc_data_processing(struct uop *op)
{
    word ins = op->undecoded.raw_instruction;
    int cp_num = BITS_SHIFT(ins, 11, 8);

    op->opcode = COPROC_DATA_PROCESSING;
    op->cond = (ins >> COND_SHIFT) & COND_MASK;
    op->coproc.raw_instruction = ins;
    op->coproc.cp_num = cp_num;

    CPU_TRACE(5, "\t\tcoproc: num %d DATA_PROCESSING ins 0x%x\n", cp_num, ins);
}

void op_coproc_load_store(struct uop *op)
{
    word ins = op->undecoded.raw_instruction;
    int cp_num = BITS_SHIFT(ins, 11, 8);

    op->opcode = COPROC_LOAD_STORE;
    op->cond = (ins >> COND_SHIFT) & COND_MASK;
    op->coproc.raw_instruction = ins;
    op->coproc.cp_num = cp_num;

    CPU_TRACE(5, "\t\tcoproc: num %d LOAD_STORE ins 0x%x\n", cp_num, ins);
}

void op_data_processing(struct uop *op)
{
    word ins = op->undecoded.raw_instruction;
    int opcode;
    enum {
        IMM_SHIFT,
        REG_SHIFT,
        IMM
    } form;
    int Rn, Rd;

    // begin setting up the translated op
    op->cond = (ins >> COND_SHIFT) & COND_MASK;

    // start decoding the fields in the instruction
    Rd = BITS_SHIFT(ins, 15, 12);
    Rn = BITS_SHIFT(ins, 19, 16);
#define S BIT(ins, 20)
    opcode = BITS_SHIFT(ins, 24, 21);

    // figure out the instruction form
    if (BITS_SHIFT(ins, 27, 25) == 1) {
        form = IMM;
    } else {
        if (BIT(ins, 4))
            form = REG_SHIFT;
        else
            form = IMM_SHIFT;
    }

    switch (form) {
        case IMM_SHIFT: {
            int Rm = BITS(ins, 3, 0);
            int shift_op = BITS_SHIFT(ins, 6, 5);
            word shift_imm = BITS_SHIFT(ins, 11, 7);

            /* look for a fast case where the barrel shifter is disabled and the third operand falls through unscathed */
            if (shift_op == 0 && shift_imm == 0) {
                if (!S && Rd == Rm && opcode == AOP_MOV) { // all registers are the same and opcode is mov
                    // nop
                    op->opcode = NOP;
                    CPU_TRACE(6, "\t\tNOP\n");
                } else if (Rd != PC && !S && opcode == AOP_MOV) { // registers are not the same and opcode is mov
                    op->opcode = MOV_REG;
                    op->simple_dp_reg.dest_reg = Rd;
                    op->simple_dp_reg.source2_reg = Rm;
                    CPU_TRACE(6, "\t\tMOV_REG: Rd %d Rm %d\n", Rd, Rm);
                } else if (S && opcode == AOP_CMP) { // compare two regs
                    op->opcode = CMP_REG_S;
                    op->simple_dp_reg.source_reg = Rn;
                    op->simple_dp_reg.source2_reg = Rm;
                    CPU_TRACE(6, "\t\tCMP_REG_S: Rn %d Rm %d\n", Rn, Rm);
                } else if (Rd != PC && !S && opcode == AOP_ADD) { // add two regs and store in a third
                    op->opcode = ADD_REG;
                    op->simple_dp_reg.dest_reg = Rd;
                    op->simple_dp_reg.source_reg = Rn;
                    op->simple_dp_reg.source2_reg = Rm;
                    CPU_TRACE(6, "\t\tADD_REG: Rd %d Rn %d Rm %d\n", Rd, Rn, Rm);
                } else {
                    CPU_TRACE(6, "\t\tIMM_REG: opcode %d (%s) Rd %d Rn %d Rm %d\n", opcode, dp_op_to_str(opcode), Rd, Rn, Rm);
                    /* translates to DATA_PROCESSING_REG */
                    if (S)
                        op->opcode = DATA_PROCESSING_REG_S;
                    else
                        op->opcode = DATA_PROCESSING_REG;
                    op->data_processing_reg.dp_opcode = opcode;
                    op->data_processing_reg.dest_reg = Rd;
                    op->data_processing_reg.source_reg = Rn;
                    op->data_processing_reg.source2_reg = Rm;
                }
                break;
            }

            /* look for variations of LSL/LSR/ASR/ROR */
            if (Rd != PC && opcode == AOP_MOV && shift_imm != 0) {
                switch (shift_op) {
                    case 0: // LSL
                        if (S)
                            op->opcode = LSL_IMM_S;
                        else
                            op->opcode = LSL_IMM;
                        op->simple_dp_imm.dest_reg = Rd;
                        op->simple_dp_imm.source_reg = Rm;
                        op->simple_dp_imm.immediate = shift_imm;
                        CPU_TRACE(6, "\t\tLSL_IMM: S Rd %d Rm %d shift_immed %d S %d\n", Rd, Rm, shift_imm, S?1:0);
                        break;
                    case 1: // LSR
                        if (S)
                            op->opcode = LSR_IMM_S;
                        else
                            op->opcode = LSR_IMM;
                        op->simple_dp_imm.dest_reg = Rd;
                        op->simple_dp_imm.source_reg = Rm;
                        op->simple_dp_imm.immediate = shift_imm;
                        CPU_TRACE(6, "\t\tLSR_IMM: S Rd %d Rm %d shift_immed %d S %d\n", Rd, Rm, shift_imm, S?1:0);
                        break;
                    case 2: // LSR
                        if (S)
                            op->opcode = ASR_IMM_S;
                        else
                            op->opcode = ASR_IMM;
                        op->simple_dp_imm.dest_reg = Rd;
                        op->simple_dp_imm.source_reg = Rm;
                        op->simple_dp_imm.immediate = shift_imm;
                        CPU_TRACE(6, "\t\tASR_IMM: S Rd %d Rm %d shift_immed %d S %d\n", Rd, Rm, shift_imm, S?1:0);
                        break;
                    case 3: // ROR
                        // XXX no immediate form right now.
                        goto imm_longform;
                }
                break;
            }

imm_longform:
            CPU_TRACE(6, "\t\tIMM_SHIFT: opcode %d (%s) Rd %d Rn %d Rm %d shift_imm %d shift_op 0x%x\n", opcode, dp_op_to_str(opcode), Rd, Rn, Rm, shift_imm, shift_op);

            /* translates to the relatively slow DATA_PROCESSING_IMM_SHIFT */
            op->opcode = DATA_PROCESSING_IMM_SHIFT;
            op->data_processing_imm_shift.dp_opcode = opcode;
            if (S)
                op->flags |= UOPDPFLAGS_S_BIT;
            op->data_processing_imm_shift.dest_reg = Rd;
            op->data_processing_imm_shift.source_reg = Rn;
            op->data_processing_imm_shift.source2_reg = Rm;
            op->data_processing_imm_shift.shift_opcode = shift_op;
            op->data_processing_imm_shift.shift_imm = shift_imm;
            break;
        }
        case REG_SHIFT: {
            int Rm = BITS(ins, 3, 0);
            word shift_op = BITS_SHIFT(ins, 6, 5);
            int Rs = BITS_SHIFT(ins, 11, 8);

            /* look for variations of LSL/LSR/ASR/ROR */
            if (Rd != PC && opcode == AOP_MOV) {
                switch (shift_op) {
                    case 0: // LSL
                        if (S)
                            op->opcode = LSL_REG_S;
                        else
                            op->opcode = LSL_REG;
                        op->simple_dp_reg.dest_reg = Rd;
                        op->simple_dp_reg.source_reg = Rm;
                        op->simple_dp_reg.source2_reg = Rs;
                        CPU_TRACE(6, "\t\tLSL_REG: S Rd %d Rm %d Rs %d S %d\n", Rd, Rm, Rs, S?1:0);
                        break;
                    case 1: // LSR
                        if (S)
                            op->opcode = LSR_REG_S;
                        else
                            op->opcode = LSR_REG;
                        op->simple_dp_reg.dest_reg = Rd;
                        op->simple_dp_reg.source_reg = Rm;
                        op->simple_dp_reg.source2_reg = Rs;
                        CPU_TRACE(6, "\t\tLSR_REG: S Rd %d Rm %d Rs %d S %d\n", Rd, Rm, Rs, S?1:0);
                        break;
                    case 2: // ASR
                        if (S)
                            op->opcode = ASR_REG_S;
                        else
                            op->opcode = ASR_REG;
                        op->simple_dp_reg.dest_reg = Rd;
                        op->simple_dp_reg.source_reg = Rm;
                        op->simple_dp_reg.source2_reg = Rs;
                        CPU_TRACE(6, "\t\tASR_REG: S Rd %d Rm %d Rs %d S %d\n", Rd, Rm, Rs, S?1:0);
                        break;
                    case 3: // ROR
                        if (S)
                            op->opcode = ROR_REG_S;
                        else
                            op->opcode = ROR_REG;
                        op->simple_dp_reg.dest_reg = Rd;
                        op->simple_dp_reg.source_reg = Rm;
                        op->simple_dp_reg.source2_reg = Rs;
                        CPU_TRACE(6, "\t\tROR_REG: S Rd %d Rm %d Rs %d S %d\n", Rd, Rm, Rs, S?1:0);
                        break;
                }
                break;
            }

            CPU_TRACE(6, "\t\tREG_SHIFT: opcode %d (%s) Rd %d Rn %d Rm %d Rs %d shift_op %d\n", opcode, dp_op_to_str(opcode), Rd, Rn, Rm, Rs, shift_op);

            /* translates to the relatively slow DATA_PROCESSING_REG_SHIFT */
            op->opcode = DATA_PROCESSING_REG_SHIFT;
            if (S)
                op->flags |= UOPDPFLAGS_S_BIT;
            op->data_processing_reg_shift.dp_opcode = opcode;
            op->data_processing_reg_shift.dest_reg = Rd;
            op->data_processing_reg_shift.source_reg = Rn;
            op->data_processing_reg_shift.source2_reg = Rm;
            op->data_processing_reg_shift.shift_opcode = shift_op;
            op->data_processing_reg_shift.shift_reg = Rs;
            break;
        }
        case IMM: {
            /* look for MOVW/MOVT instructions, which are encoded as a special form of immedate TST/CMP with no S bit */
            if (get_isa() >= ARM_V7) {
                if (!S && opcode == AOP_TST) { // MOVW
                    op->opcode = MOV_IMM;
                    op->simple_dp_imm.dest_reg = Rd;
                    word immed = (BITS_SHIFT(ins, 19, 16) << 12) | BITS(ins, 11, 0);
                    op->simple_dp_imm.immediate = immed;

                    CPU_TRACE(6, "\t\tMOV_IMM: Rd %d immed 0x%x\n", Rd, immed);
                    break;
                } else if (!S && opcode == AOP_CMP) { // MOVT
                    op->opcode = MOV_IMM_TOP;
                    op->simple_dp_imm.dest_reg = Rd;
                    word immed = (BITS_SHIFT(ins, 19, 16) << 12) | BITS(ins, 11, 0);
                    op->simple_dp_imm.immediate = immed << 16;

                    CPU_TRACE(6, "\t\tMOV_IMM_TOP: Rd %d immed 0x%x\n", Rd, op->simple_dp_imm.immediate);
                    break;
                }
            }

            word immed = BITS(ins, 7, 0);
            int rotate_imm = BITS_SHIFT(ins, 11, 8);

            if (rotate_imm != 0) {
                rotate_imm *= 2;
                immed = ROR(immed, rotate_imm);
            }

            // look for further special cases
            if (Rd != PC && !S && opcode == AOP_MOV) { // MOV
                op->opcode = MOV_IMM;
                op->simple_dp_imm.dest_reg = Rd;
                op->simple_dp_imm.immediate = immed;

                CPU_TRACE(6, "\t\tMOV_IMM: Rd %d immed %d\n", Rd, immed);
            } else if (Rd != PC && !S && opcode == AOP_MVN) { // MVN
                op->opcode = MOV_IMM;
                op->simple_dp_imm.dest_reg = Rd;
                op->simple_dp_imm.immediate = ~immed;

                CPU_TRACE(6, "\t\tMVN_IMM: Rd %d immed %d\n", Rd, immed);
            } else if (S && opcode == AOP_CMP) { // CMP
                op->opcode = CMP_IMM_S;
                op->simple_dp_imm.source_reg = Rn;
                op->simple_dp_imm.immediate = immed;

                CPU_TRACE(6, "\t\tCMP_IMM_S: Rn %d immed %d\n", Rn, immed);
            } else if (S && opcode == AOP_CMN) { // CMN
                op->opcode = CMP_IMM_S;
                op->simple_dp_imm.source_reg = Rn;
                op->simple_dp_imm.immediate = -immed;

                CPU_TRACE(6, "\t\tCMN_IMM_S: Rn %d immed %d\n", Rn, immed);
            } else if (Rd != PC && !S && opcode == AOP_ADD) { // ADD
                op->opcode = ADD_IMM;
                op->simple_dp_imm.dest_reg = Rd;
                op->simple_dp_imm.source_reg = Rn;
                op->simple_dp_imm.immediate = immed;

                CPU_TRACE(6, "\t\tADD_IMM: Rd %d Rn %d immed %d\n", Rd, Rn, immed);
            } else if (Rd != PC && !S && opcode == AOP_SUB) { // SUB
                op->opcode = ADD_IMM;
                op->simple_dp_imm.dest_reg = Rd;
                op->simple_dp_imm.source_reg = Rn;
                op->simple_dp_imm.immediate = -immed;

                CPU_TRACE(6, "\t\tSUB_IMM: Rd %d Rn %d immed %d\n", Rd, Rn, immed);
                break;
            } else if (Rd != PC && !S && opcode == AOP_AND) { // AND
                op->opcode = AND_IMM;
                op->simple_dp_imm.dest_reg = Rd;
                op->simple_dp_imm.source_reg = Rn;
                op->simple_dp_imm.immediate = immed;

                CPU_TRACE(6, "\t\tAND_IMM: Rd %d Rn %d immed %d\n", Rd, Rn, immed);
                break;
            } else if (Rd != PC && !S && opcode == AOP_ORR) { // ORR
                op->opcode = ORR_IMM;
                op->simple_dp_imm.dest_reg = Rd;
                op->simple_dp_imm.source_reg = Rn;
                op->simple_dp_imm.immediate = immed;

                CPU_TRACE(6, "\t\tORR_IMM: Rd %d Rn %d immed %d\n", Rd, Rn, immed);
                break;
            } else {
                /* translates to more generic DATA_PROCESSING_IMM */
                if (S)
                    op->opcode = DATA_PROCESSING_IMM_S;
                else
                    op->opcode = DATA_PROCESSING_IMM;
                op->data_processing_imm.dp_opcode = opcode;
                op->data_processing_imm.dest_reg = Rd;
                op->data_processing_imm.source_reg = Rn;
                op->data_processing_imm.immediate = immed;

                if (rotate_imm != 0) {
                    op->flags |= UOPDPFLAGS_SET_CARRY_FROM_SHIFTER;
                    if (op->data_processing_imm.immediate & 0x80000000)
                        op->flags |= UOPDPFLAGS_CARRY_FROM_SHIFTER;
                }
                CPU_TRACE(6, "\t\tIMM: opcode %d (%s) Rd %d Rn %d rotate_imm %d immed %d\n", opcode, dp_op_to_str(opcode), Rd, Rn, rotate_imm, op->data_processing_imm.immediate);
            }
            break;
        }
    }

#undef S
}

void op_mul(struct uop *op)
{
    word ins = op->undecoded.raw_instruction;
    int Rd, Rn, Rs, Rm;

    // decode the fields in the instruction
    Rm = BITS(ins, 3, 0);
    Rs = BITS_SHIFT(ins, 11, 8);
    Rn = BITS_SHIFT(ins, 15, 12);
    Rd = BITS_SHIFT(ins, 19, 16);
#define S BIT(ins, 20)
#define A BIT(ins, 21)

    CPU_TRACE(5, "\t\tmul: A %d, S %d, Rd %d, Rn %d, Rs %d, Rm %d\n",
              A?1:0, S?1:0, Rd, Rn, Rs, Rm);

    // translate the instruction
    op->opcode = MULTIPLY;
    op->cond = (ins >> COND_SHIFT) & COND_MASK;
    op->mul.dest_reg = Rd;
    op->mul.source_reg = Rm;
    op->mul.source2_reg = Rs;
    op->mul.accum_reg = Rn;
    if (S)
        op->flags |= UOPMULFLAGS_S_BIT;
    if (A)
        op->flags |= UOPMULFLAGS_ACCUMULATE;
#undef S
#undef A
}

void op_mull(struct uop *op)
{
    word ins = op->undecoded.raw_instruction;
    int RdHi, RdLo, Rs, Rm;

    // decode the instruction
    Rm = BITS(ins, 3, 0);
    Rs = BITS_SHIFT(ins, 11, 8);
    RdLo = BITS_SHIFT(ins, 15, 12);
    RdHi = BITS_SHIFT(ins, 19, 16);
#define S BIT(ins, 20)
#define A BIT(ins, 21)
#define U BIT(ins, 22)

    CPU_TRACE(5, "\t\tmull: A %d, U %d, S %d, RdHi %d, RdLo %d, Rs %d, Rm %d\n",
              A?1:0, U?1:0, S?1:0, RdHi, RdLo, Rs, Rm);

    // translate the instruction
    op->opcode = MULTIPLY_LONG;
    op->cond = (ins >> COND_SHIFT) & COND_MASK;
    op->mull.destlo_reg = RdLo;
    op->mull.desthi_reg = RdHi;
    op->mull.source_reg = Rm;
    op->mull.source2_reg = Rs;
    if (S)
        op->flags |= UOPMULFLAGS_S_BIT;
    if (A)
        op->flags |= UOPMULFLAGS_ACCUMULATE;
    if (U) // signed bit, confusingly named U
        op->flags |= UOPMULFLAGS_SIGNED;

#undef U
#undef A
#undef S
}

void op_swap(struct uop *op)
{
    word ins = op->undecoded.raw_instruction;
    int Rn, Rd, Rm;

    // decode the fields in the instruction
    Rm = BITS(ins, 3, 0);
    Rd = BITS_SHIFT(ins, 15, 12);
    Rn = BITS_SHIFT(ins, 19, 16);
#define B BIT_SHIFT(ins, 22)

    CPU_TRACE(5, "\t\tswp: B %d, Rn %d, Rd %d, Rm %\n", B?1:0, Rn, Rd, Rm);

    op->opcode = SWAP;
    op->cond = (ins >> COND_SHIFT) & COND_MASK;
    op->swp.dest_reg = Rd;
    op->swp.source_reg = Rm;
    op->swp.mem_reg = Rn;
    op->swp.b = B;

#if COUNT_CYCLES
    // XXX cycle count
    if (get_core() == ARM7) {
        add_to_perf_counter(CYCLE_COUNT, 3);
    } else { /* if(get_core() >= ARM9) */
        add_to_perf_counter(CYCLE_COUNT, 1);
        // XXX schedule interlock
    }
#endif
#if COUNT_ARM_OPS
    inc_perf_counter(OP_MISC);
#endif
#undef B
}

void op_load_store(struct uop *op)
{
    word ins = op->undecoded.raw_instruction;
    int Rn, Rd;
    armaddr_t offset;

    // start decoding the fields in the instruction
    Rd = BITS_SHIFT(ins, 15, 12);
    Rn = BITS_SHIFT(ins, 19, 16);
#define L BIT(ins, 20)
#define W BIT(ins, 21)
#define B BIT(ins, 22)
#define U BIT(ins, 23)
#define P BIT(ins, 24)
#define SR BIT(ins, 25)

    // look for an unhandled form (user mode access)
    if (!P && W) {
        panic_cpu("op_load_store: user mode access unhandled\n");
    }

    // look for a particular case that decodes to a very simple uop
    if (L && Rn == PC && !SR) {
        // LOAD_IMMEDIATE
        if (!B)
            op->opcode = LOAD_IMMEDIATE_WORD;
        else
            op->opcode = LOAD_IMMEDIATE_BYTE;
        op->cond = (ins >> COND_SHIFT) & COND_MASK;
        op->load_immediate.target_reg = Rd;
        offset = ins & 0xfff;
        if (!U)
            offset = -offset;
        op->load_immediate.address = get_reg(PC) + offset;

        CPU_TRACE(5, "\t\tload_store: L %d, W %d, B %d, Rd %d addr 0x%x\n",
                  L?1:0, W?1:0, B?1:0, Rd, op->load_immediate.address);

        return;
    }

    if (!SR) {
        // a simple offset, translates to LOAD_IMMEDIATE_OFFSET
        if (L)
            op->opcode = LOAD_IMMEDIATE_OFFSET;
        else
            op->opcode = STORE_IMMEDIATE_OFFSET;
        op->cond = (ins >> COND_SHIFT) & COND_MASK;
        if (!P)
            op->flags |= UOPLSFLAGS_POSTINDEX;
        if (W || !P)
            op->flags |= UOPLSFLAGS_WRITEBACK;
        if (B)
            op->flags |= UOPLSFLAGS_SIZE_BYTE;
        else
            op->flags |= UOPLSFLAGS_SIZE_WORD;
        op->load_store_immediate_offset.target_reg = Rd;
        op->load_store_immediate_offset.source_reg = Rn;
        offset = ins & 0xfff;
        if (!U)
            offset = -offset;
        op->load_store_immediate_offset.offset = offset;

        CPU_TRACE(5, "\t\tload_store: L %d, W %d, B %d, Rd %d, Rn %d offset 0x%x\n",
                  L?1:0, W?1:0, B?1:0, Rd, Rn, offset);

        return;
    } else {
        int shift_imm = BITS_SHIFT(ins, 11, 7);
        int shift_op = BITS_SHIFT(ins, 6, 5);
        int Rm = BITS(ins, 3, 0);

        // translates to LOAD_SCALED_REG_OFFSET
        if (L)
            op->opcode = LOAD_SCALED_REG_OFFSET;
        else
            op->opcode = STORE_SCALED_REG_OFFSET;
        op->cond = (ins >> COND_SHIFT) & COND_MASK;
        if (!P)
            op->flags |= UOPLSFLAGS_POSTINDEX;
        if (W || !P)
            op->flags |= UOPLSFLAGS_WRITEBACK;
        if (!U)
            op->flags |= UOPLSFLAGS_NEGATE_OFFSET;
        if (B)
            op->flags |= UOPLSFLAGS_SIZE_BYTE;
        else
            op->flags |= UOPLSFLAGS_SIZE_WORD;
        op->load_store_scaled_reg_offset.shift_immediate = shift_imm;
        op->load_store_scaled_reg_offset.target_reg = Rd;
        op->load_store_scaled_reg_offset.source_reg = Rn;
        op->load_store_scaled_reg_offset.source2_reg = Rm;
        op->load_store_scaled_reg_offset.shift_op = shift_op;

        CPU_TRACE(5, "\t\tload_store: L %d, W %d, U %d, B %d, Rd %d, Rn %d, Rm %d, shift_imm 0x%x, shift_op %d\n",
                  L?1:0, W?1:0, U?1:0, B?1:0, Rd, Rn, Rm, shift_imm, shift_op);
    }
#undef SR
#undef L
#undef W
#undef B
#undef U
#undef P
}

void op_misc_load_store(struct uop *op)
{
    word ins = op->undecoded.raw_instruction;
    int Rn, Rd;

    // start decoding the fields in the instruction
    Rd = BITS_SHIFT(ins, 15, 12);
    Rn = BITS_SHIFT(ins, 19, 16);
    word D = 0; // doubleword
#define H BIT(ins, 5)
    word S = BIT(ins, 6);
    word L = BIT(ins, 20);
#define W BIT(ins, 21)
#define I BIT(ins, 22)
#define U BIT(ins, 23)
#define P BIT(ins, 24)

    // look for the special doubleword case
    if (!L && S) {
        // in armv4 this would have been signed halfword/byte store, which doesn't make any sense
        // ldrd/strd uses this previously unused decoding
        D = 1;
        L = !H;
        S = 0;

        // check for undefined case. Rd cannot be odd
        if (Rd & 1) {
            op_undefined(op);
        }
    }

    if (D && get_isa() < ARM_V5e) {
        op_undefined(op);
        return;
    }

    // immediate or register offset
    if (I) {
        // immediate offset
        armaddr_t offset = BITS(ins, 3, 0);
        offset |= BITS(ins, 11, 8) >> 4;

        // look for a particular case that decodes to a very simple uop
        if (L && Rn == PC && !D) {
            // LOAD_IMMEDIATE
            if (H)
                op->opcode = LOAD_IMMEDIATE_HALFWORD;
            else
                op->opcode = LOAD_IMMEDIATE_BYTE;
            op->cond = (ins >> COND_SHIFT) & COND_MASK;
            op->load_immediate.target_reg = Rd;

            if (!U)
                offset = -offset;

            op->load_immediate.address = get_reg(PC) + offset;

            CPU_TRACE(5, "\t\tmisc_load_store: IMMEDIATE L %d, W %d, D %d, H %d, Rd %d addr 0x%x\n",
                      L?1:0, W?1:0, D?1:0, H?1:0, Rd, op->load_immediate.address);
        } else {
            // LOAD_IMMEDIATE_OFFSET, STORE_IMMEDIATE_OFFSET
            if (L)
                op->opcode = LOAD_IMMEDIATE_OFFSET;
            else
                op->opcode = STORE_IMMEDIATE_OFFSET;
            op->cond = (ins >> COND_SHIFT) & COND_MASK;
            if (!P)
                op->flags |= UOPLSFLAGS_POSTINDEX;
            if (W || !P)
                op->flags |= UOPLSFLAGS_WRITEBACK;
            if (D)
                op->flags |= UOPLSFLAGS_SIZE_DWORD;
            else if (H)
                op->flags |= UOPLSFLAGS_SIZE_HALFWORD;
            else
                op->flags |= UOPLSFLAGS_SIZE_BYTE;
            if (S)
                op->flags |= UOPLSFLAGS_SIGN_EXTEND;
            op->load_store_immediate_offset.target_reg = Rd;
            op->load_store_immediate_offset.source_reg = Rn;

            if (!U)
                offset = -offset;
            op->load_store_immediate_offset.offset = offset;

            CPU_TRACE(5, "\t\tmisc_load_store: IMMEDIATE_OFFSET L %d, W %d, P %d, D %d, H %d, S %d, Rd %d Rn %d offset 0x%x\n",
                      L?1:0, W?1:0, P?1:0, D?1:0, H?1:0, S?1:0, Rd, Rn, op->load_store_immediate_offset.offset);
        }
    } else {
        // register offset
        int Rm = BITS(ins, 3, 0);

        // translates to LOAD_SCALED_REG_OFFSET
        if (L)
            op->opcode = LOAD_SCALED_REG_OFFSET;
        else
            op->opcode = STORE_SCALED_REG_OFFSET;
        op->cond = (ins >> COND_SHIFT) & COND_MASK;
        if (!P)
            op->flags |= UOPLSFLAGS_POSTINDEX;
        if (W || !P)
            op->flags |= UOPLSFLAGS_WRITEBACK;
        if (!U)
            op->flags |= UOPLSFLAGS_NEGATE_OFFSET;
        if (D)
            op->flags |= UOPLSFLAGS_SIZE_DWORD;
        else if (H)
            op->flags |= UOPLSFLAGS_SIZE_HALFWORD;
        else
            op->flags |= UOPLSFLAGS_SIZE_BYTE;
        if (S)
            op->flags |= UOPLSFLAGS_SIGN_EXTEND;
        op->load_store_scaled_reg_offset.shift_op = 0; // LSL
        op->load_store_scaled_reg_offset.shift_immediate = 0;
        op->load_store_scaled_reg_offset.target_reg = Rd;
        op->load_store_scaled_reg_offset.source_reg = Rn;
        op->load_store_scaled_reg_offset.source2_reg = Rm;

        CPU_TRACE(5, "\t\tmisc_load_store: REG_OFFSET L %d, W %d, P %d, D %d, H %d, S %d, Rd %d Rn %d Rm %d\n",
                  L?1:0, W?1:0, P?1:0, D?1:0, H?1:0, S?1:0, Rd, Rn, Rm);
    }
#undef H
#undef W
#undef I
#undef U
#undef P
}

void op_load_store_multiple(struct uop *op)
{
    word ins = op->undecoded.raw_instruction;
    int Rn;
    int base_offset;
    int writeback_offset;
    halfword reg_list;
    int reg_count;
    int S;
    int load_cpsr;

    // start decoding the fields in the instruction
    reg_list = BITS(ins, 15, 0);
    Rn = BITS_SHIFT(ins, 19, 16);
#define L BIT(ins, 20)
#define W BIT(ins, 21)
    S = BIT(ins, 22);
#define U BIT(ins, 23)
#define P BIT(ins, 24)

    // count the number of regs we're going to need to load/store
    static const char nibble_bit_count[16] = {
        0, // 0000
        1, // 0001
        1, // 0010
        2, // 0011
        1, // 0100
        2, // 0101
        2, // 0110
        3, // 0111
        1, // 1000
        2, // 1001
        2, // 1010
        3, // 1011
        2, // 1100
        3, // 1101
        3, // 1110
        4, // 1111
    };
    reg_count = nibble_bit_count[reg_list & 0xf] + nibble_bit_count[(reg_list >> 4) & 0xf];
    reg_count += nibble_bit_count[(reg_list >> 8) & 0xf] + nibble_bit_count[(reg_list >> 12) & 0xf];

    // calculate the address
    if (U) {
        // upwards
        if (P)
            base_offset = 4; // base address not included
        else
            base_offset = 0;
        writeback_offset = reg_count * 4;
    } else {
        // downwards
        if (P)
            base_offset = -(reg_count * 4); // base address not included
        else
            base_offset = -(reg_count * 4) + 4;
        writeback_offset = -(reg_count * 4);
    }

    // start filling out the translated instruction
    op->cond = (ins >> COND_SHIFT) & COND_MASK;
    op->load_store_multiple.base_offset = base_offset;
    op->load_store_multiple.writeback_offset = writeback_offset;
    op->load_store_multiple.reg_bitmap = reg_list;
    op->load_store_multiple.reg_count = reg_count;
    op->load_store_multiple.base_reg = Rn;
    if (W)
        op->flags |= UOPLSMFLAGS_WRITEBACK;

    // figure out which opcode it should be
    load_cpsr = 0;
    if (L) {
        if (S && (reg_list & 0x8000)) {
            // pc is a target, load normally and load cpsr from spsr
            load_cpsr = 1;
            S = 0;
        }

        if (S)
            op->opcode = LOAD_MULTIPLE_S;
        else
            op->opcode = LOAD_MULTIPLE;

        if (load_cpsr)
            op->flags |= UOPLSMFLAGS_LOAD_CPSR;
    } else {
        if (S)
            op->opcode = STORE_MULTIPLE_S;
        else
            op->opcode = STORE_MULTIPLE;
    }

    CPU_TRACE(5, "\t\tload_store_multiple: L %d W %d S %d load_cpsr %d Rn %d bitmap 0x%x offset %d, wb_offset %d\n",
              L?1:0, W?1:0, S?1:0, load_cpsr, Rn, op->load_store_multiple.reg_bitmap,
              op->load_store_multiple.base_offset, op->load_store_multiple.writeback_offset);
#undef L
#undef W
#undef U
#undef P
}

void op_bfx(struct uop *op)
{
    word ins = op->undecoded.raw_instruction;
    int Rd, Rn;
    int S;

    // only supported on ARMv6+
    if (get_isa() < ARM_V6) {
        op_undefined(op);
        return;
    }

    // decode the instruction
    S = !BIT(ins, 22);
    Rd = BITS_SHIFT(ins, 15, 12);
    Rn = BITS(ins, 3, 0);
    word width = BITS_SHIFT(ins, 20, 16) + 1;
    word lsb = BITS_SHIFT(ins, 11, 7);

    // translate
    op->opcode = BFX;
    op->cond = (ins >> COND_SHIFT) & COND_MASK;
    op->flags = S ? UOPBFX_S_BIT : 0;
    op->bfx.dest_reg = Rd;
    op->bfx.source_reg = Rn;
    op->bfx.shift = lsb;

    if (width + lsb >= 32) {
        // unpredictable
        width = 32 - lsb;
    }
    op->bfx.signpos = width - 1;
    op->bfx.mask = 0xffffffff >> (32 - width);

    CPU_TRACE(5, "\t\top_bfx: Rd %d Rn %d, S %d, width %d, lsb %d, signpos %d\n", Rd, Rn, S ? 1 : 0, width, lsb, op->bfx.signpos);
}

void op_extend(struct uop *op)
{
    word ins = op->undecoded.raw_instruction;

    // only supported on ARMv6+
    if (get_isa() < ARM_V6) {
        op_undefined(op);
        return;
    }

    // decode the instruction
    int S = !BIT(ins, 22);
    int Rd = BITS_SHIFT(ins, 15, 12);
    int Rm = BITS_SHIFT(ins, 3, 0);
    int Rn = BITS_SHIFT(ins, 19, 16);
    word size = BITS_SHIFT(ins, 21, 20); // byte, half, byte 16-bit
    word rotate = BITS_SHIFT(ins, 11, 10); // 0, 8, 16, 24 bits
    int A = (Rn != 15) ? UOPEXTEND_A_BIT : 0;

    // translate
    op->opcode = EXTEND;
    op->cond = (ins >> COND_SHIFT) & COND_MASK;
    op->flags = S ? UOPEXTEND_S_BIT : 0;
    op->flags |= A ? UOPEXTEND_A_BIT : 0;
    op->extend.dest_reg = Rd;
    op->extend.source_reg = Rm;
    op->extend.add_reg = Rn;
    op->extend.size = size;
    op->extend.rotate = rotate;

    CPU_TRACE(5, "\t\top_extend: Rd %d Rm %d Rn %d, S %d, A %d, size %u, rotate %u\n", Rd, Rm, Rn, S ? 1 : 0, A ? 1 : 0, size, rotate);
}

void op_load_store_exclusive(struct uop *op)
{
    word ins = op->undecoded.raw_instruction;

    // only supported on ARMv6+
    if (get_isa() < ARM_V6) {
        op_undefined(op);
        return;
    }

    // decode the instruction
    int Rd = BITS_SHIFT(ins, 15, 12);
    int Rt = BITS_SHIFT(ins, 3, 0);
    int Rn = BITS_SHIFT(ins, 19, 16);

    CPU_TRACE(5, "\t\top_load_store_exclusive: Rd %d Rt %d Rn %d\n", Rd, Rt, Rn);

    panic_cpu("WAHT");
}

