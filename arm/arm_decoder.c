/*
 * Copyright (c) 2005-2015 Travis Geiselbrecht
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
    unsigned int op1 = BITS_SHIFT(op->undecoded.raw_instruction, 24, 20);
    unsigned int op2 = BITS_SHIFT(op->undecoded.raw_instruction, 7, 4);

    /* look for special cases (various miscellaneous forms) */
    /* Table A5-2 */
    //printf("group 0: op1 0x%x, op2 0x%x\n", op1, op2);
    if ((op1 & 0b11001) == 0b10000) {
        if ((op2 & 0b1000) == 0) {
            /* miscellaneous instructions */
            unsigned int B = BIT(op->undecoded.raw_instruction, 9);
            unsigned int _op = BITS_SHIFT(op->undecoded.raw_instruction, 22, 21);
            op1 = BITS_SHIFT(op->undecoded.raw_instruction, 19, 16);
            op2 = BITS_SHIFT(op->undecoded.raw_instruction, 6, 4);

            //printf("misc, op 0x%x, op1 0x%x, op2 0x%x, B %u\n", _op, op1, op2, B);
            switch (op2) {
                case 0b000:
                    if (B && (_op & 0b01) == 0b00) {
                        /* MRS (banked register) */
                        bad_decode(op);
                    } else if (B && (_op & 0b01) == 0b01) {
                        /* MSR (banked register) */
                        bad_decode(op);
                    } else if (!B && (_op & 0b01) == 0b00) {
                        /* MRS register, system */
                        op_mrs(op);
                    } else if (!B && _op == 0b01 && (op1 & 0b0011) == 0) {
                        /* MSR register, application level */
                        op_msr(op);
                    } else if (!B && _op == 0b01 && (op1 & 0b0011) == 0b0001) {
                        /* MSR register, system */
                        op_msr(op);
                    } else if (!B && _op == 0b01 && (op1 & 0b0010) == 0b0010) {
                        /* MSR register, system */
                        op_msr(op);
                    } else {
                        op_undefined(op);
                    }
                    break;
                case 0b001:
                    if (_op == 0b01)
                        op_bx(op); // bx
                    else if (_op == 0b11)
                        op_clz(op);
                    else
                        op_undefined(op);
                    break;
                case 0b010:
                    if (_op == 0b01)
                        /* BXJ */
                        bad_decode(op);
                    else
                        op_undefined(op);
                    break;
                case 0b011:
                    if (_op == 0b01)
                        op_bx(op); // blx
                    else
                        op_undefined(op);
                    break;
                case 0b101:
                    bad_decode(op); // DSP add/subtracts go here (qadd, qsub, qdadd, qdsub)
                    break;
                case 0b110:
                    if (_op == 0b11)
                        bad_decode(op); // eret
                    else
                        op_undefined(op);
                    break;
                case 0b111:
                    if (_op == 0b01)
                        op_bkpt(op); // bkpt
                    else if (_op == 0b10)
                        bad_decode(op); // hvc
                    else if (_op == 0b11)
                        bad_decode(op); // smc
                    else
                        op_undefined(op);
                    break;
                default:
                    op_undefined(op);
            }
        } else if ((op2 & 0b1001) == 0b1000) {
            /* halfword multiply and multiply accumulate */
            bad_decode(op);
        } else {
            op_undefined(op);
        }
    } else if ((op2 & 0b0001) == 0b0000) {
        op_data_processing(op); // data processing register
    } else if ((op2 & 0b1001) == 0b0001) {
        op_data_processing(op); // data processing register-shifted register
    } else if ((op1 & 0b10000) == 0b00000 && op2 == 0b1001) {
        // multiply and multiply accumulate
        op1 &= 0b1111;
        if ((op1 & 0b1110) == 0b0000)
            op_mul(op); // mul
        else if ((op1 & 0b1110) == 0b0010)
            op_mul(op); // mla
        else if (op1 == 0b0100)
            bad_decode(op); // umaal
        else if (op1 == 0b0110)
            op_mul(op); // mls
        else if ((op1 & 0b1110) == 0b1000)
            op_mull(op); // umull
        else if ((op1 & 0b1110) == 0b1010)
            op_mull(op); // umlal
        else if ((op1 & 0b1110) == 0b1100)
            op_mull(op); // smull
        else if ((op1 & 0b1110) == 0b1110)
            op_mull(op); // smlal
        else
            op_undefined(op);
    } else if ((op1 & 0b10000) == 0b10000 && op2 == 0b1001) {
        /* synchronization primitives */
        op1 &= 0b1111;

        if ((op1 & 0b1011) == 0)
            op_swap(op); // swp/swpb
        else
            op_load_store_exclusive(op); // ldrex/strex, byte/halfword/word/double variants
    } else if ((op1 & 0b10011) == 0b00010 && (op2 & 0b1101) == 0b1101) {
        /* extra load/store instructions */
        op_misc_load_store(op); // load store halfword/doubleword
    } else if ((op1 & 0b10010) == 0b00010 && op2 == 0b1011) {
        /* extra load/store instructions, unpriviledged */
        bad_decode(op);
    } else if ((op1 & 0b10011) == 0b00011 && (op2 & 0b1101) == 0b1101) {
        /* extra load/store instructions, unpriviledged */
        bad_decode(op);
    } else if (op2 == 0b1011) {
        /* extra load/store instructions */
        op_misc_load_store(op); // load store halfword/doubleword
    } else if ((op2 & 0b1101) == 0b1101) {
        /* extra load/store instructions */
        op_misc_load_store(op); // load store halfword/doubleword
    } else {
        op_undefined(op);
    }

#if 0
    switch (op->undecoded.raw_instruction & ((3<<23)|(1<<20)|(1<<7)|(1<<4))) {
        default:
            op_data_processing(op); // data processing immediate shift and register shift
            break;
        // cases of bits 24:23 == 2 and bit 20 == 0 and bit 7 == 0
        case (2<<23):
        case (2<<23)|(1<<4): { // miscellaneous instructions (Figure 3-3, page A3-4).
            // also section 3.13.3, page A3-30 (control instruction extension space)
            op1 = BITS_SHIFT(op->undecoded.raw_instruction, 22, 21);

            // switch based off of bits 7:4
            switch (op->undecoded.raw_instruction & (0x7 << 4)) {
                case (0<<4):
                    if (op1 & 1)
                        op_msr(op); // msr
                    else
                        op_mrs(op); // mrs
                    break;
                case (1<<4):
                    switch (op1) {
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
                    if (op1 == 1) {
                        op_bx(op); // blx, register form
                    } else {
                        op_undefined(op); // undefined
                    }
                    break;
                case (5<<4):
                    bad_decode(op); // XXX DSP add/subtracts go here (qadd, qsub, qdadd, qdsub)
                    break;
                case (7<<4):
                    if (op1 == 1) {
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
        case (3<<23)|(1<<20)|(1<<7)|(1<<4): { // multiplies, extra load/stores (Figure 3-2, page A3-3)
            switch (op->undecoded.raw_instruction & (3<<5)) { // bits 5 and 6
                case 0: // multiply, multiply long, swap
                    switch (op->undecoded.raw_instruction & (3<<23)) {
                        case 0:
                            op_mul(op); // mul/mla
                            break;
                        case (1<<23):
                            op_mull(op); // umull/smull/umlal/smlal
                            break;
                        case (2<<23):
                            op_swap(op); // swp/swpb
                            break;
                        default:
                            op_undefined(op);
                    }
                    break;
                default: // load/store halfward and load/store two words
                    op_misc_load_store(op); // load store halfword/doubleword
                    break;
            }
            break;
        }
    }
#endif
}

// opcode[27:25] == 0b001
static void prim_group_1_decode(struct uop *op)
{
    /* look for special cases, most of this group is immediate data processing */
    /* Table A5-2 */
    switch (BITS_SHIFT(op->undecoded.raw_instruction, 24, 20)) {
        case 0b10010:
        case 0b10110: // MSR, immediate form
            op_msr(op);
            break;
        case 0b10000:
        case 0b10100:
            // undefined prior to v6t2
            if (get_isa() < ARM_V6) {
                op_undefined(op);
                break;
            }
            // movw/movt
            op_data_processing(op);
            break;
        default:
            op_data_processing(op);
            break;
    }
}

// opcode[27:25] == 0b011
static void prim_group_3_decode(struct uop *op)
{
    /* look for a particular undefined form */
    if (BIT(op->undecoded.raw_instruction, 4)) {
        if (get_isa() < ARM_V6) {
            op_undefined(op);
            return;
        }

        // media instructions
        // table A5-16 on pg A5-209
        word op1 = BITS_SHIFT(op->undecoded.raw_instruction, 24, 20);
        word op2 = BITS_SHIFT(op->undecoded.raw_instruction, 7, 5);

        if ((op1 & 0b11100) == 0) {
            // parallel addition and subtraction, signed
            bad_decode(op);
        } else if ((op1 & 0b11100) == 0b00100) {
            // parallel addition and subtraction, unsigned
            bad_decode(op);
        } else if ((op1 & 0b11000) == 0b01000) {
            // packing, unpacking, saturation, and reversal (table A5-19)
            op1 = BITS_SHIFT(op->undecoded.raw_instruction, 22, 20);

            switch (op1) {
                case 0b000:
                    if ((op2 & 0b001) == 0) {
                        // pack halfword (PKH)
                        bad_decode(op);
                    } else if (op2 == 0b011) {
                        // signed extend byte 16-bit (SXTB16 and SXTAB16)
                        op_extend(op);
                    } else if (op2 == 0b101) {
                        // select bytes (SEL)
                        bad_decode(op);
                    } else {
                        op_undefined(op);
                    }
                    break;
                case 0b010:
                    if (op2 == 0b001) {
                        // signed saturate, two 16-bit (SSAT16)
                        bad_decode(op);
                    } else if (op2 == 0b011) {
                        // signed extend byte (SXTB and SXTAB)
                        op_extend(op);
                    } else if ((op2 & 0b001) == 0) {
                        // signed saturate (SSAT)
                        bad_decode(op);
                    } else {
                        op_undefined(op);
                    }
                    break;
                case 0b011:
                    if (op2 == 0b001) {
                        // byte reverse word (REV)
                        bad_decode(op);
                    } else if (op2 == 0b011) {
                        // signed extend halfword (SXTH and SXTAH)
                        op_extend(op);
                    } else if (op2 == 0b101) {
                        // byte-reverse packed halfword (REV16)
                        bad_decode(op);
                    } else if ((op2 & 0b001) == 0) {
                        // signed saturate (SSAT)
                        bad_decode(op);
                    } else {
                        op_undefined(op);
                    }
                    break;
                case 0b100:
                    if (op2 == 0b011) {
                        // unsigned extend byte 16-bit (UXTB16 and UXTAB16)
                        op_extend(op);
                    } else {
                        op_undefined(op);
                    }
                    break;
                case 0b110:
                    if (op2 == 0b001) {
                        // unsigned saturate, two 16-bit (USAT16)
                        bad_decode(op);
                    } else if (op2 == 0b011) {
                        // unsigned extend byte (UXTB and UXTAB)
                        op_extend(op);
                    } else if ((op2 & 0b001) == 0) {
                        // unsigned saturate (USAT)
                        bad_decode(op);
                    } else {
                        op_undefined(op);
                    }
                    break;
                case 0b111:
                    if (op2 == 0b001) {
                        // reverse bits (RBIT)
                        bad_decode(op);
                    } else if (op2 == 0b011) {
                        // unsigned extend halfword (UXTH and UXTAH)
                        op_extend(op);
                    } else if (op2 == 0b101) {
                        // byte-reverse signed halfword (REVSH)
                        bad_decode(op);
                    } else if ((op2 & 0b001) == 0) {
                        // signed saturate (USAT)
                        bad_decode(op);
                    } else {
                        op_undefined(op);
                    }
                    break;
                default:
                    op_undefined(op);
            }
        } else if ((op1 & 0b11000) == 0b10000) {
            // signed multiply, signed and unsigned divide
            bad_decode(op);
        } else if (op1 == 0b11000 && op2 == 0) {
            // usad8 or usada8
            bad_decode(op);
        } else if ((op1 & 0b11010) == 0b11010 && ((op2 & 0b011) == 0b010)) {
            op_bfx(op); // sbfx/ubfx
        } else if ((op1 & 0b11110) == 0b11100 && ((op2 & 0b011) == 0b000)) {
            // bfc/bfi
            bad_decode(op);
        } else if (op1 == 0b1111 && op2 == 0b111) {
            op_undefined(op); // permanently undefined
        } else {
            // unhandled stuff
            bad_decode(op);
        }
    } else {
        op_load_store(op); // general load/store
    }
}

// opcode[27:25] == 0b110
static void prim_group_6_decode(struct uop *op)
{
    /* look for the coprocessor instruction extension space */
    if ((BITS(op->undecoded.raw_instruction, 24, 23) | BIT(op->undecoded.raw_instruction, 21)) == 0) {
        if (BIT(op->undecoded.raw_instruction, 22)) {
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
    if (((op->undecoded.raw_instruction >> COND_SHIFT) & COND_MASK) == COND_SPECIAL) {
        op_undefined(op);
        return;
    }

    /* switch between swi and coprocessor */
    if (BIT(op->undecoded.raw_instruction, 24)) {
        op_swi(op);
    } else {
        if (BIT(op->undecoded.raw_instruction, 4)) {
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
    prim_group_3_decode, // load/store register offset and v6+ media instructions
    op_load_store_multiple, // load/store multiple
    op_branch,  // branch op
    prim_group_6_decode, // coprocessor load/store and double reg transfers
    prim_group_7_decode, // coprocessor data processing/register transfers, swi, undefined instructions
};

void arm_decode_into_uop(struct uop *op)
{
    word ins = op->undecoded.raw_instruction;
    /* look for the unconditional instruction extension space */
    /* all of these are unpredictable in V4 */
    if (BITS_SHIFT(ins, 31, 27) == 0b11110) {
        /* figure A5-217, ARM DDI 0406C.b */
        uint32_t op1 = BITS_SHIFT(ins, 26, 20);
        uint32_t Rn = BITS_SHIFT(ins, 19, 16);
        uint32_t op2 = BITS_SHIFT(ins, 7, 4);

        if (op1 == 0b0010000) {
            if (((op2 & 0b0010) == 0) && ((Rn & 0x1) == 0)) {
                op_cps(op);
            } else if (op2 == 0 && ((Rn & 0x1) == 1)) {
                // XXX setend
                bad_decode(op);
            } else {
                op_undefined(op);
            }
        } else if ((op1 & 0b1100000) == 0b0100000) {
            // XXX Advanced SIMD processing
            bad_decode(op);
        } else if ((op1 & 0b1110001) == 0b1000000) {
            // XXX Advanced SIMD element or structure load/store instructions
            bad_decode(op);
        } else if ((op1 & 0b1110111) == 0b1000001) {
            op_nop(op); // unallocated memory hint (nop)
        } else if ((op1 & 0b1110111) == 0b1000101) {
            op_pld(op); // v7 immediate pld
        } else if ((op1 & 0b1110111) == 0b1010001) {
            op_pld(op); // mp ext pld
        } else if ((op1 & 0b1110111) == 0b1010101) {
            op_pld(op); // v5 pld
        } else if (op1 == 0b1010111) {
            switch (op2) {
                case 0b0001: // CLREX
                    op_nop(op); // op_clrex(op);
                    break;
                case 0b0100: // DSB
                    op_nop(op);
                    break;
                case 0b0101: // DMB
                    op_nop(op);
                    break;
                case 0b0110: // ISB
                    op_nop(op);
                    break;
                default:
                    op_undefined(op);
                    break;
            }
        } else if ((op1 & 0b1111011) == 0b1011011) {
            op_nop(op); // unallocated memory hint (nop)
        } else if ((op1 & 0b1110111) == 0b1100101 && ((op2 & 1) == 0)) {
            op_pld(op); // pli
        } else if ((op1 & 0b1110111) == 0b1100001 && ((op2 & 1) == 0)) {
            op_pld(op); // preload with intent to write
        } else if ((op1 & 0b1110111) == 0b1110101 && ((op2 & 1) == 0)) {
            op_pld(op); // v5 pld
        } else if (op1 == 0b1111111 && op2 == 0b1111) {
            op_undefined(op); // permanently undefined
        } else {
            op_undefined(op);
        }
    } else if (BITS_SHIFT(op->undecoded.raw_instruction, 31, 28) == 0b1111) {
        /* figure A5-216, ARM DDI 0406C.b */
        uint32_t op1 = BITS_SHIFT(ins, 27, 20);
        //uint32_t op2 = BIT_SHIFT(ins, 4);

        if ((op1 & 0b11100101) == 0b10000100) {
            op_srs(op); // SRS
        } else if ((op1 & 0b11100101) == 0b10000001) {
            op_rfe(op); // RFE
        } else if ((op1 & 0b11100000) == 0b10100000) {
            op_branch(op); // blx (address form)
        } else if ((op1 & 0xe0) == 0xc0) {
            // XXX stc2/ldc2
            bad_decode(op);
        } else if ((op1 & 0xf0) == 0xe0) {
            // XXX cdp2/mcr2/mrc2
            bad_decode(op);
        } else {
            // XXX probably missing a couple of cases
            op_undefined(op);
        }
    } else {
        ins_group_table[BITS_SHIFT(op->undecoded.raw_instruction, 27, 25)](op);
    }
}


