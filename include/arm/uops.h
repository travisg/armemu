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
#ifndef __ARM_UOPS_H
#define __ARM_UOPS_H

/* opcodes supported by the uop interpreter */
enum uop_opcode {
    DECODE_ME_ARM = 0,
    DECODE_ME_THUMB,
    NOP,
    B_IMMEDIATE,
    B_IMMEDIATE_LOCAL,          // immediate branch, local to this codepage, has to be in the same mode (ARM or THUMB)
    B_REG,
    B_REG_OFFSET,               // add an immediate offset to a register (second half of thumb bl/blx)
    LOAD_IMMEDIATE_WORD,        // fast forms for pc relative loads
    LOAD_IMMEDIATE_HALFWORD,    // "
    LOAD_IMMEDIATE_BYTE,        // "
    LOAD_IMMEDIATE_OFFSET,      // add an immediate to base register
    LOAD_SCALED_REG_OFFSET,     // add base register to shifted second register
    STORE_IMMEDIATE_OFFSET,     // add an immediate to base register
    STORE_SCALED_REG_OFFSET,    // add base register to shifted second register

    LOAD_MULTIPLE,              // simple multiple load, no S bit
    LOAD_MULTIPLE_S,
    STORE_MULTIPLE,             // simple multiple store, no S bit
    STORE_MULTIPLE_S,

    // data processing, ARM style
    DATA_PROCESSING_IMM,        // plain instruction, no barrel shifter, no condition flag update, immediate operand
    DATA_PROCESSING_REG,        // plain instruction, no barrel shifter, no condition flag update, register operand
    DATA_PROCESSING_IMM_S,      // S bit set, update condition flags
    DATA_PROCESSING_REG_S,      // S bit set, update condition flags
    DATA_PROCESSING_IMM_SHIFT,  // barrel shifter involved, immediate operands to shifter, S bit may be involved
    DATA_PROCESSING_REG_SHIFT,  // barrel shifter involved, register operands to shifter, S bit may be involved

    // special versions of some of the above instructions
    MOV_IMM,                    // mov and mvn
    MOV_IMM_NZ,                 // mov and mvn, sets NZ condition bits
    MOV_REG,                    // mov
    CMP_IMM_S,                  // cmp and cmn, sets full conditions
    CMP_REG_S,                  // cmp, sets full conditions
    CMN_REG_S,                  // cmn, sets full conditions
    TST_REG_S,                  // cmn, sets full conditions
    ADD_IMM,                    // add and sub
    ADD_IMM_S,                  // add and sub, sets full conditions
    ADD_REG,                    // add
    ADD_REG_S,                  // add, sets full conditions
    ADC_REG_S,                  // add with carry, sets full conditions
    SUB_REG_S,                  // subtract, sets full conditions
    SBC_REG_S,                  // subtract with carry, sets full conditions

    AND_IMM,                    // and with immediate value, no conditions
    ORR_IMM,                    // orr with immediate value, no conditions
    ORR_REG_S,                  // orr, sets NZ condition bits

    LSL_IMM,                    // logical left shift by immediate
    LSL_IMM_S,                  // logical left shift by immediate, sets full conditions
    LSL_REG,                    // logical left shift by register
    LSL_REG_S,                  // logical left shift by register, sets full conditions
    LSR_IMM,                    // logical right shift by immediate
    LSR_IMM_S,                  // logical right shift by immediate, sets full conditions
    LSR_REG,                    // logical right shift by register
    LSR_REG_S,                  // logical right shift by register, sets full conditions
    ASR_IMM,                    // arithmetic right shift by immediate
    ASR_IMM_S,                  // arithmetic right shift by immediate, sets full conditions
    ASR_REG,                    // arithmetic right shift by register
    ASR_REG_S,                  // arithmetic right shift by register, sets full conditions
    ROR_REG,                    // rotate right by register
    ROR_REG_S,                  // rotate right by register, sets full conditions

    AND_REG_S,                  // and, sets NZ condition bits
    EOR_REG_S,                  // exclusive or, sets NZ condition bits
    BIC_REG_S,                  // and with complement, sets NZ condition bits
    NEG_REG_S,                  // subtract from 0, sets full condition bits
    MVN_REG_S,                  // bitwise reverse, sets NZ condition bits

    // multiply variants
    MULTIPLY,
    MULTIPLY_LONG,

    SWAP,

    // count leading zeros
    COUNT_LEADING_ZEROS,

    // move from/to status register
    MOVE_TO_SR_IMM,
    MOVE_TO_SR_REG,
    MOVE_FROM_SR,

    // various exceptions
    UNDEFINED,
    SWI,
    BKPT,

    // coprocessor
    COPROC_REG_TRANSFER,
    COPROC_DOUBLE_REG_TRANSFER,
    COPROC_DATA_PROCESSING,
    COPROC_LOAD_STORE,

    MAX_UOP_OPCODE,
};

/* description for the internal opcode format and decoder routines */
struct uop {
    halfword opcode;
    byte cond; // 4 bits of condition
    byte flags; // up to 8 flags
    union {
        struct {
            // undecoded, arm or thumb
            word raw_instruction;
        } undecoded;

        // branch opcodes
#define UOPBFLAGS_LINK                  0x1
#define UOPBFLAGS_SETTHUMB_ALWAYS       0x2
#define UOPBFLAGS_UNSETTHUMB_ALWAYS     0x4
#define UOPBFLAGS_SETTHUMB_COND         0x8 // set the thumb bit conditionally on the new value loaded
        struct {
            word target;
            word link_target;
            struct uop_codepage *target_cp; // once it's executed, cache a copy of the target codepage, if it's a nonlocal jump
        } b_immediate;
        struct {
            word reg;
            word link_offset;
        } b_reg;
        struct {
            word reg;
            word link_offset;
            word offset;
        } b_reg_offset;

        // load/store opcodes
#define UOPLSFLAGS_WRITEBACK        0x1 // write the computed address back into the source register, set on postindex ops
#define UOPLSFLAGS_NEGATE_OFFSET    0x2 // negate the offset before applying it
#define UOPLSFLAGS_SIGN_EXTEND  0x4
#define UOPLSFLAGS_POSTINDEX        0x8
#define UOPLSFLAGS_SIZE_DWORD       (0<<4)
#define UOPLSFLAGS_SIZE_WORD        (1<<4)
#define UOPLSFLAGS_SIZE_HALFWORD    (2<<4)
#define UOPLSFLAGS_SIZE_BYTE        (3<<4)
#define UOPLSFLAGS_SIZE_MASK        (3<<4)
        struct {
            word address;       // address is precomputed (pc relative loads)
            byte target_reg;
        } load_immediate;
        struct {
            word offset;        // offset is precomputed
            byte target_reg;
            byte source_reg;
        } load_store_immediate_offset;
        struct {
            word shift_immediate;
            byte target_reg;
            byte source_reg;
            byte source2_reg;
            byte shift_op; // LSL, LSR, ASR, ROR
        } load_store_scaled_reg_offset;

        // load/store multiple
#define UOPLSMFLAGS_WRITEBACK               0x1 // update the base reg with the writeback address
#define UOPLSMFLAGS_LOAD_CPSR               0x2 // in one form of load multiple, if the S bit is set and the PC is in the list, do a normal load + load cpsr from spsr
        struct {
            int16_t  base_offset;       // add to base reg to get starting address (may be negative)
            int16_t  writeback_offset;  // add to base reg to get writeback address (may be negative)
            halfword reg_bitmap;
            byte     reg_count;
            byte     base_reg;
        } load_store_multiple;

        // data processing opcodes
#define UOPDPFLAGS_SET_CARRY_FROM_SHIFTER   0x1 // use the following bit to set the carry bit if writeback
#define UOPDPFLAGS_CARRY_FROM_SHIFTER       0x2 // carry out of the barrel shifter, in case it was factored out and store as an immediate
#define UOPDPFLAGS_S_BIT                    0x4 // in the case of the barrel shifter instructions, conditional writeback is optional based on the S bit
        struct {
            byte dp_opcode; // and, or, add, etc
            byte dest_reg;
            byte source_reg;
            word immediate;
        } data_processing_imm;
        struct {
            byte dp_opcode; // and, or, add, etc
            byte dest_reg;
            byte source_reg;
            byte source2_reg;
        } data_processing_reg;
        struct {
            byte dp_opcode; // and, or, add, etc
            byte dest_reg;
            byte source_reg;
            byte source2_reg;
            byte shift_opcode;
            byte shift_imm;
        } data_processing_imm_shift;
        struct {
            byte dp_opcode; // and, or, add, etc
            byte dest_reg;
            byte source_reg;
            byte source2_reg;
            byte shift_opcode;
            byte shift_reg;
        } data_processing_reg_shift;

        // special versions of some of the above instructions
        struct {
            word immediate;
            byte dest_reg;
            byte source_reg;
        } simple_dp_imm;
        struct {
            byte dest_reg;
            byte source_reg;
            byte source2_reg;
        } simple_dp_reg;

        // multiply
#define UOPMULFLAGS_S_BIT                   0x1 // set NZ condition
#define UOPMULFLAGS_ACCUMULATE              0x2 // add the contents of the accum reg to the product
#define UOPMULFLAGS_SIGNED                  0x4 // signed long multiply (only defined on long multiples)
        struct {
            byte dest_reg;
            byte source_reg;
            byte source2_reg;
            byte accum_reg;
        } mul;
        struct {
            byte destlo_reg;
            byte desthi_reg;
            byte source_reg;
            byte source2_reg;
        } mull;

        struct {
            byte dest_reg;
            byte source_reg;
            byte mem_reg;
            byte b;
        } swp;

        // count leading zeros
        struct {
            byte dest_reg;
            byte source_reg;
        } count_leading_zeros;

        // move from/to status register
#define UOPMSR_R_BIT                        0x1 // access to spsr instead of cpsr
        struct {
            word immediate;
            word field_mask;
        } move_to_sr_imm;
        struct {
            byte reg;
            word field_mask;
        } move_to_sr_reg;
        struct {
            byte reg;
        } move_from_sr;

        // coprocessor instructions
        struct {
            word raw_instruction;           // the coprocessor parses the instruction
            byte cp_num;
        } coproc;

        struct sizing {
            // space it out to 16 bytes
            unsigned int data0;
            unsigned int data1;
            unsigned int data2;
        } _sizing;
    };
};

#define CODEPAGE_HASHSIZE 1024

#define NUM_CODEPAGE_INS_ARM    (MMU_PAGESIZE / 4)
#define NUM_CODEPAGE_INS_THUMB  (MMU_PAGESIZE / 2)

/* a page of uops at a time */
struct uop_codepage {
    struct uop_codepage *next;
    armaddr_t address;

    bool thumb; /* arm or thumb */

    /*
     * how much to push the pc up by at every instruction.
     * also pc_inc * 2 is how much r15 will be ahead of the current instruction.
     */
    int pc_inc;
    int pc_shift; // number of bits the real pc should be shifted to get to the codepage index (2 for arm, 1 for thumb)

    struct uop ops[0]; /* we will allocate a different amount of space if it's arm or thumb */
};

/* main dispatch routine, returns on internal abort */
int uop_dispatch_loop(void);

const char *uop_opcode_to_str(int opcode);

void uop_init(void);

#endif
