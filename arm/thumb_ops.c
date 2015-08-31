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

void thumb_op_shift_imm(struct uop *op)
{
    halfword ins = op->undecoded.raw_instruction;
    int opcode;
    int Rd, Rm;
    int immed;

    // decode
    opcode = BITS_SHIFT(ins, 12, 11);
    immed = BITS_SHIFT(ins, 10, 6);
    Rm = BITS_SHIFT(ins, 5, 3);
    Rd = BITS(ins, 2, 0);

    CPU_TRACE(5, "\t\tthumb_op_shift_imm: opcode %d, immed %d, Rm %d, Rd %d\n",
              opcode, immed, Rm, Rd);

    // common opcode stuff
    op->cond = COND_AL;
    op->simple_dp_imm.dest_reg = Rd;
    op->simple_dp_imm.source_reg = Rm;
    op->simple_dp_imm.immediate = immed;

    switch (opcode) {
        default:
        case 0x0: // LSL
            op->opcode = LSL_IMM_S;
            break;
        case 0x1: // LSR
            op->opcode = LSR_IMM_S;
            break;
        case 0x2: // ASR
            op->opcode = ASR_IMM_S;
            break;
    }
}

void thumb_op_add_sub_3reg(struct uop *op)
{
    halfword ins = op->undecoded.raw_instruction;
    int Rn, Rd;
    int opcode;

    // decode
    opcode = BIT(ins, 9);
    Rn = BITS_SHIFT(ins, 5, 3);
    Rd = BITS_SHIFT(ins, 2, 0);

    op->cond = COND_AL;

    // figure out if we're immediate or reg
    if (BIT(ins, 10)) {
        // immediate
        word immediate = BITS_SHIFT(ins, 8, 6);

        CPU_TRACE(5, "\t\tthumb_op_add_sub_3reg: sub %d, Rd %d, Rn %d, immediate %d\n",
                  opcode, Rd, Rn, immediate);

        if (immediate == 0) {
            // special form of this instruction that just moves
            op->opcode = MOV_REG;
            op->simple_dp_reg.dest_reg = Rd;
            op->simple_dp_reg.source2_reg = Rn;
        } else {
            if (opcode) // subtract
                immediate = -immediate;
            op->opcode = ADD_IMM_S;
            op->simple_dp_imm.immediate = immediate;
            op->simple_dp_imm.dest_reg = Rd;
            op->simple_dp_imm.source_reg = Rn;
        }
    } else {
        // register
        int Rm = BITS_SHIFT(ins, 8, 6);

        CPU_TRACE(5, "\t\tthumb_op_add_sub_3reg: sub %d, Rd %d, Rn %d, Rm %d\n",
                  opcode, Rd, Rn, Rm);

        if (opcode)
            op->opcode = SUB_REG_S;
        else
            op->opcode = ADD_REG_S;
        op->simple_dp_reg.dest_reg = Rd;
        op->simple_dp_reg.source_reg = Rn;
        op->simple_dp_reg.source2_reg = Rm;
    }
}

void thumb_op_add_sub_large_immediate(struct uop *op)
{
    halfword ins = op->undecoded.raw_instruction;
    int RdRn;
    int opcode;
    reg_t immediate;

    // decode
    opcode = BITS_SHIFT(ins, 12, 11);
    RdRn = BITS_SHIFT(ins, 10, 8);
    immediate = BITS(ins, 7, 0);

    CPU_TRACE(5, "\t\tthumb_op_add_sub_large_immediate: opcode %d, Rd/Rn %d, immediate %d\n",
              opcode, RdRn, immediate);

    // common opcode stuff
    op->cond = COND_AL;
    op->simple_dp_imm.dest_reg = RdRn;
    op->simple_dp_imm.source_reg = RdRn;
    op->simple_dp_imm.immediate = immediate;

    // do the op
    switch (opcode) {
        default:
        case 0: // MOV
            op->opcode = MOV_IMM_NZ;
            break;
        case 1: // CMP
            op->opcode = CMP_IMM_S;
            break;
        case 2: // ADD
            op->opcode = ADD_IMM_S;
            break;
        case 3: // SUB
            op->opcode = ADD_IMM_S;
            op->simple_dp_imm.immediate = -op->simple_dp_imm.immediate;
            break;
    }
}

void thumb_op_data_processing(struct uop *op)
{
    halfword ins = op->undecoded.raw_instruction;
    int RmRs, RdRn;
    int opcode;

    // decode
    opcode = BITS_SHIFT(ins, 9, 6);
    RmRs = BITS_SHIFT(ins, 5, 3);
    RdRn = BITS(ins, 2, 0);

    CPU_TRACE(5, "\t\tthumb_op_data_processing: opcode %d, RmRs %d, RdRn %d\n", opcode, RmRs, RdRn);

    // common opcode stuff
    op->cond = COND_AL;
    op->simple_dp_reg.dest_reg = RdRn;
    op->simple_dp_reg.source_reg = RdRn;
    op->simple_dp_reg.source2_reg = RmRs;

    // do the op
    switch (opcode) {
        default:
        case 0x0: // AND
            op->opcode = AND_REG_S;
            break;
        case 0x1: // EOR
            op->opcode = EOR_REG_S;
            break;
        case 0x2: // LSL
            op->opcode = LSL_REG_S;
            break;
        case 0x3: // LSR
            op->opcode = LSR_REG_S;
            break;
        case 0x4: // ASR
            op->opcode = ASR_REG_S;
            break;
        case 0x6: // SBC
            op->opcode = SBC_REG_S;
            break;
        case 0x5: // ADC
            op->opcode = ADC_REG_S;
            break;
        case 0x7: // ROR
            op->opcode = ROR_REG_S;
            break;
        case 0x8: // TST
            op->opcode = TST_REG_S;
            break;
        case 0x9: // NEG
            op->opcode = NEG_REG_S;
            break;
        case 0xa: // CMP
            op->opcode = CMP_REG_S;
            break;
        case 0xb: // CMN
            op->opcode = CMN_REG_S;
            break;
        case 0xc: // ORR
            op->opcode = ORR_REG_S;
            break;
        case 0xd: // MUL
            // multiply has it's own uop format
            op->opcode = MULTIPLY;
            op->mul.dest_reg = RdRn;
            op->mul.source_reg = RdRn;
            op->mul.source2_reg = RmRs;
            op->flags = UOPMULFLAGS_S_BIT;
            break;
        case 0xe: // BIC
            op->opcode = BIC_REG_S;
            break;
        case 0xf: // MVN
            op->opcode = MVN_REG_S;
            break;
    }
}

// ADD/CMP/MOV from/to high registers
void thumb_op_special_data_processing(struct uop *op)
{
    halfword ins = op->undecoded.raw_instruction;
    int RdRn, Rm;
    int opcode;

    // decode
    opcode = BITS_SHIFT(ins, 9, 8);
    Rm = BITS_SHIFT(ins, 6, 3); // picks up the H2 bit as the top bit
    RdRn = BITS(ins, 2, 0) | ((ins >> 4) & 0x08); // picks up H1 as the top bit

    CPU_TRACE(5, "\t\tthumb_op_special_data_processing: opcode %d, Rd/Rn %d, Rm %d\n",
              opcode, RdRn, Rm);

    // start to emit the common parts of the opcode
    op->cond = COND_AL;
    op->simple_dp_reg.dest_reg = RdRn;
    op->simple_dp_reg.source_reg = RdRn;
    op->simple_dp_reg.source2_reg = Rm;

    // do the op
    switch (opcode) {
        case 0: // ADD
            op->opcode = ADD_REG;
            break;
        case 1: // CMP
            op->opcode = CMP_REG_S;
            break;
        case 2: // MOV
            op->opcode = MOV_REG;
            break;
        default:
            op->opcode = UNDEFINED;
    }
}

// load relative to PC (+1024)
void thumb_op_literal_load(struct uop *op)
{
    halfword ins = op->undecoded.raw_instruction;
    int Rd;
    int immed;
    armaddr_t addr;

    // decode
    Rd = BITS_SHIFT(ins, 10, 8);
    immed = BITS(ins, 7, 0);
    immed *= 4;
    addr = (get_reg(PC) & 0xfffffffc) + immed;

    CPU_TRACE(5, "\t\tthumb_op_literal_load: Rd %d, immed %d, addr 0x%x\n",
              Rd, immed, addr);

    // emit the opcode
    op->opcode = LOAD_IMMEDIATE_WORD;
    op->cond = COND_AL;
    op->load_immediate.target_reg = Rd;
    op->load_immediate.address = addr;
}

void thump_op_load_store_register(struct uop *op)
{
    halfword ins = op->undecoded.raw_instruction;
    int opcode;
    int Rm, Rn, Rd;

    // decode
    opcode = BITS_SHIFT(ins, 11, 9);
    Rm = BITS_SHIFT(ins, 8, 6);
    Rn = BITS_SHIFT(ins, 5, 3);
    Rd = BITS(ins, 2, 0);

    // start constructing the uop
    op->cond = COND_AL;
    op->load_store_scaled_reg_offset.target_reg = Rd;
    op->load_store_scaled_reg_offset.source_reg = Rn;
    op->load_store_scaled_reg_offset.source2_reg = Rm;
    op->load_store_scaled_reg_offset.shift_op = 0; // LSL
    op->load_store_scaled_reg_offset.shift_immediate = 0; // disabling the shifter
    op->flags = 0;

    // figure out what we're doing based off the opcode
    switch (opcode) {
        default:
        case 0: // STR
            op->opcode = STORE_SCALED_REG_OFFSET;
            op->flags |= UOPLSFLAGS_SIZE_WORD;
            break;
        case 1: // STRH
            op->opcode = STORE_SCALED_REG_OFFSET;
            op->flags |= UOPLSFLAGS_SIZE_HALFWORD;
            break;
        case 2: // STRB
            op->opcode = STORE_SCALED_REG_OFFSET;
            op->flags |= UOPLSFLAGS_SIZE_BYTE;
            break;
        case 3: // LDRSB
            op->opcode = LOAD_SCALED_REG_OFFSET;
            op->flags |= UOPLSFLAGS_SIZE_BYTE;
            op->flags |= UOPLSFLAGS_SIGN_EXTEND;
            break;
        case 4: // LDR
            op->opcode = LOAD_SCALED_REG_OFFSET;
            op->flags |= UOPLSFLAGS_SIZE_WORD;
            break;
        case 5: // LDRH
            op->opcode = LOAD_SCALED_REG_OFFSET;
            op->flags |= UOPLSFLAGS_SIZE_HALFWORD;
            break;
        case 6: // LDRB
            op->opcode = LOAD_SCALED_REG_OFFSET;
            op->flags |= UOPLSFLAGS_SIZE_BYTE;
            break;
        case 7: // LDRSH
            op->opcode = LOAD_SCALED_REG_OFFSET;
            op->flags |= UOPLSFLAGS_SIZE_HALFWORD;
            op->flags |= UOPLSFLAGS_SIGN_EXTEND;
            break;
    }

    CPU_TRACE(5, "\t\tthumb_op_load_store_register: op %d Rd %d, Rm %d, Rn %d\n",
              opcode, Rd, Rm, Rn);
}

void thumb_op_load_store_immediate_offset(struct uop *op)
{
    halfword ins = op->undecoded.raw_instruction;
    int Rn, Rd;
    int immed;
    int size; // 4, 2, 1
    int L;

    // decode
    immed = BITS_SHIFT(ins, 10, 6);
    Rn = BITS_SHIFT(ins, 5, 3);
    Rd = BITS(ins, 2, 0);
    L = BIT(ins, 11);

    // figure out the size (based off the primary opcode and a bit inside one of the forms)
    if (BITS_SHIFT(ins, 15, 12) == 6) {
        size = 4;
        immed *= 4;
    } else if (BITS_SHIFT(ins, 15, 12) == 7) {
        size = 1;
    } else {
        size = 2;
        immed *= 2;
    }

    // start constructing the uop
    op->cond = COND_AL;
    op->flags = 0;
    op->load_store_immediate_offset.offset = immed;
    op->load_store_immediate_offset.target_reg = Rd;
    op->load_store_immediate_offset.source_reg = Rn;

    if (L) {
        op->opcode = LOAD_IMMEDIATE_OFFSET;
    } else {
        op->opcode = STORE_IMMEDIATE_OFFSET;
    }

    if (size == 4) {
        op->flags |= UOPLSFLAGS_SIZE_WORD;
    } else if (size == 2) {
        op->flags |= UOPLSFLAGS_SIZE_HALFWORD;
    } else {
        op->flags |= UOPLSFLAGS_SIZE_BYTE;
    }

    CPU_TRACE(5, "\t\tthumb_op_load_store_immediate: size %d, L %d, Rn %d, Rd %d, immed %d\n",
              size, L, Rn, Rd, immed);
}

void thumb_op_load_store_from_stack(struct uop *op)
{
    halfword ins = op->undecoded.raw_instruction;
    int Rd;
    int immed;
    int L;

    // decode
    immed = BITS(ins, 7, 0);
    Rd = BITS_SHIFT(ins, 10, 8);
    L = BIT(ins, 11);
    immed *= 4;

    CPU_TRACE(5, "\t\tthumb_op_load_store_from_stack: L %d, Rd %d, immed %d\n",
              L, Rd, immed);

    // start building the uop
    op->cond = COND_AL;
    op->flags = UOPLSFLAGS_SIZE_WORD;
    op->load_store_immediate_offset.offset = immed;
    op->load_store_immediate_offset.target_reg = Rd;
    op->load_store_immediate_offset.source_reg = SP;

    if (L)
        op->opcode = LOAD_IMMEDIATE_OFFSET;
    else
        op->opcode = STORE_IMMEDIATE_OFFSET;
}

void thumb_op_load_store_multiple(struct uop *op)
{
    halfword ins = op->undecoded.raw_instruction;
    int L;
    int Rn;
    int reg_list;
    int reg_count;

    // decode the instruction
    reg_list = BITS(ins, 7, 0);
    Rn = BITS_SHIFT(ins, 10, 8);
    L = BIT(ins, 11);

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
    reg_count = nibble_bit_count[BITS(reg_list, 3, 0)] + nibble_bit_count[BITS_SHIFT(reg_list, 7, 4)];

    CPU_TRACE(5, "\t\tthumb_op_load_store_multiple: L %d Rn %d reg_list 0x%x\n", L?1:0, Rn, reg_list);

    // emit the uop
    op->cond = COND_AL;
    op->load_store_multiple.base_offset = 0;                    // always upwards, base included
    op->load_store_multiple.writeback_offset = reg_count * 4;
    op->load_store_multiple.reg_bitmap = reg_list;
    op->load_store_multiple.reg_count = reg_count;
    op->load_store_multiple.base_reg = Rn;
    op->flags = UOPLSMFLAGS_WRITEBACK; // writeback is unconditional

    if (L) {
        op->opcode = LOAD_MULTIPLE;
    } else {
        op->opcode = STORE_MULTIPLE;
    }
}

void thumb_op_add_to_sp_pc(struct uop *op)
{
    halfword ins = op->undecoded.raw_instruction;
    int Rd;
    int immed;
    int sp;

    // decode
    sp = BIT(ins, 11);
    Rd = BITS_SHIFT(ins, 10, 8);
    immed = BITS(ins, 7, 0);
    immed *= 4;

    CPU_TRACE(5, "\t\tthumb_op_add_to_sp_pc: SP %d, Rd %d, immed %d\n", sp?1:0, Rd, immed);

    op->cond = COND_AL;
    op->flags = 0;

    if (sp) {
        op->opcode = ADD_IMM;
        op->simple_dp_imm.immediate = immed;
        op->simple_dp_imm.dest_reg = Rd;
        op->simple_dp_imm.source_reg = SP;
    } else {
        panic_cpu("thumb_op_add_to_sp_pc unimplemented form\n");
        put_reg(Rd, (get_reg(15) & 0xfffffffc) + immed);
    }
}

void thumb_op_adjust_sp(struct uop *op)
{
    halfword ins = op->undecoded.raw_instruction;
    int sub;
    reg_t immed_7;

    // decode
    sub = BIT(ins, 7);
    immed_7 = BITS(ins, 6, 0);
    immed_7 *= 4;

    CPU_TRACE(5, "\t\tthumb_op_adjust_sp: sub %d, immed %d\n", sub?1:0, immed_7);

    op->cond = COND_AL;
    op->flags = 0;
    op->opcode = ADD_IMM;
    op->simple_dp_imm.dest_reg = SP;
    op->simple_dp_imm.source_reg = SP;
    if (sub)
        op->simple_dp_imm.immediate = -immed_7;
    else
        op->simple_dp_imm.immediate = immed_7;
}

void thumb_op_push_pop_regs(struct uop *op)
{
    halfword ins = op->undecoded.raw_instruction;
    int L;
    int R;
    int reg_list;
    armaddr_t base_offset;
    armaddr_t writeback_offset;
    int reg_count;

    // decode the instruction
    L = BIT(ins, 11);
    R = BIT(ins, 8);
    reg_list = BITS(ins, 7, 0);

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
    reg_count = nibble_bit_count[BITS(reg_list, 3, 0)] + nibble_bit_count[BITS_SHIFT(reg_list, 7, 4)];
    if (R) {
        reg_count++; // we're gonna load/store lr also
        reg_list |= (1 << LR);
    }

    // figure out the starting and stopping address
    if (L) {
        // upwards, base address included
        base_offset = 0;
        writeback_offset = reg_count * 4;
    } else {
        // downwards, base address not included
        base_offset = -(reg_count * 4);
        writeback_offset = base_offset;
    }

    CPU_TRACE(5, "\t\tthumb_op_push_pop_regs: L %d R %d reg_list 0x%x, base_offset %d, wb_offset %d\n",
              L?1:0, R?1:0, reg_list, base_offset, writeback_offset);

    // emit the uop
    op->cond = COND_AL;
    op->load_store_multiple.base_offset = base_offset;
    op->load_store_multiple.writeback_offset = writeback_offset;
    op->load_store_multiple.reg_bitmap = reg_list;
    op->load_store_multiple.reg_count = reg_count;
    op->load_store_multiple.base_reg = SP; // 13
    op->flags = UOPLSMFLAGS_WRITEBACK; // writeback is unconditional

    if (L) {
        op->opcode = LOAD_MULTIPLE;
    } else {
        op->opcode = STORE_MULTIPLE;
    }
}

void thumb_op_software_breakpoint(struct uop *op)
{
    halfword ins = op->undecoded.raw_instruction;

    op->opcode = BKPT;
    op->cond = COND_AL;

    CPU_TRACE(5, "thumb breakpoint, ins 0x%x\n", ins);
}

void thumb_op_undefined(struct uop *op)
{
    halfword ins = op->undecoded.raw_instruction;

    op->opcode = UNDEFINED;
    op->cond = COND_AL;

    CPU_TRACE(5, "undefined thumb instruction 0x%x\n", ins);
}

void thumb_op_swi(struct uop *op)
{
    halfword ins = op->undecoded.raw_instruction;

    op->opcode = SWI;
    op->cond = COND_AL;

    CPU_TRACE(5, "\t\tswi 0x%x\n", ins & 0x000000ff);
}

void thumb_op_conditional_branch(struct uop *op)
{
    halfword ins = op->undecoded.raw_instruction;
    int cond;
    int immed;
    armaddr_t pc;
    armaddr_t target;

    // decode
    cond = BITS_SHIFT(ins, 11, 8);
    immed = BITS(ins, 7, 0);
    immed <<= 1;
    immed = SIGN_EXTEND(immed, 8);
    pc = get_reg(PC);
    target = pc + immed;

    CPU_TRACE(5, "\t\tthumb_op_conditional_branch: cond 0x%x, immediate %d, target %d\n", cond, immed, target);

    // build the opcode
    op->cond = cond;
    op->flags = 0;
    op->b_immediate.target = target;
    op->b_immediate.link_target = 0;
    op->b_immediate.target_cp = NULL;
    if ((op->b_immediate.target >> MMU_PAGESIZE_SHIFT) == ((pc - 8) >> MMU_PAGESIZE_SHIFT))
        op->opcode = B_IMMEDIATE_LOCAL; // it's within the current codepage
    else
        op->opcode = B_IMMEDIATE; // outside the current codepage
    op->cond = cond;
}

void thumb_op_branch(struct uop *op)
{
    halfword ins = op->undecoded.raw_instruction;
    word immed_11;
    armaddr_t pc;
    armaddr_t target;

    // decode
    immed_11 = BITS(ins, 10, 0);
    immed_11 <<= 1;
    immed_11 = SIGN_EXTEND(immed_11, 11);
    pc = get_reg(PC);
    target = pc + immed_11;

    CPU_TRACE(5, "\t\tthumb_op_branch: immediate %d, target 0x%08x\n", immed_11, target);

    // build the opcode
    op->cond = COND_AL;
    op->flags = 0;
    op->b_immediate.target = target;
    op->b_immediate.link_target = 0;
    op->b_immediate.target_cp = NULL;
    if ((op->b_immediate.target >> MMU_PAGESIZE_SHIFT) == ((pc - 8) >> MMU_PAGESIZE_SHIFT))
        op->opcode = B_IMMEDIATE_LOCAL; // it's within the current codepage
    else
        op->opcode = B_IMMEDIATE; // outside the current codepage
}

void thumb_op_bx_reg(struct uop *op)
{
    halfword ins = op->undecoded.raw_instruction;
    int Rm;
    int L;

    // decode
    Rm = BITS_SHIFT(ins, 6, 3); // includes the H2 bit
    L = BIT(ins, 7); // H1 bit

    CPU_TRACE(5, "\t\tthumb_op_bx_reg: L %d, Rm %d\n", L?1:0, Rm);

    // build the opcode
    op->cond = COND_AL;
    op->opcode = B_REG;
    op->flags = UOPBFLAGS_SETTHUMB_COND;
    op->b_reg.reg = Rm;

    // if this is blx...
    if (L) {
        op->b_reg.link_offset = -2;
        op->flags |= UOPBFLAGS_LINK;
    }
}

void thumb_op_bl_blx(struct uop *op)
{
    halfword ins = op->undecoded.raw_instruction;
    word offset;
    int H;

    // decode the instruction
    offset = BITS(ins, 10, 0);
    H = BITS_SHIFT(ins, 12, 11);

    // start building uop
    op->flags = 0;
    op->cond = COND_AL;

    // it's either first or second half depending on H
    switch (H) {
        case 2: // first half of bl/blx
            // shift over 12 bits and sign extend
            offset <<= 12;
            offset = SIGN_EXTEND(offset, 22);

            CPU_TRACE(5, "\t\tthumb_op_bl_blx (1st half): offset 0x%08x\n", offset);

            // emit as an immediate mov to LR (r14)
            op->opcode = MOV_IMM;
            op->simple_dp_imm.immediate = get_reg(PC) + offset;
            op->simple_dp_imm.dest_reg = LR;
            op->simple_dp_imm.source_reg = 0;
            break;
        case 3: // second half of bl
            offset <<= 1;
            CPU_TRACE(5, "\t\tthumb_op_bl_blx (2nd half of bl): offset 0x%08x\n", offset);

            op->opcode = B_REG_OFFSET;
            op->b_reg_offset.reg = LR;
            op->b_reg_offset.link_offset = -2;
            op->b_reg_offset.offset = offset;
            op->flags |= UOPBFLAGS_LINK;
            break;
        case 1: // second half of blx
            offset <<= 1;
            CPU_TRACE(5, "\t\tthumb_op_bl_blx (2nd half of blx): offset 0x%08x\n", offset);

            op->opcode = B_REG_OFFSET;
            op->b_reg_offset.reg = LR;
            op->b_reg_offset.link_offset = -2;
            op->b_reg_offset.offset = offset;
            op->flags |= UOPBFLAGS_LINK;
            op->flags |= UOPBFLAGS_UNSETTHUMB_ALWAYS; // switch back to arm unconditionally
            break;
        case 0: // invalid, covered by the unconditional branch instruction
        default:
            panic_cpu("bad decode of bl/blx instruction\n");
    }
}
