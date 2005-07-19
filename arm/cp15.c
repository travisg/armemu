#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <stdio.h>
#include <unistd.h>

#include <sys/sys.h>
#include <arm/arm.h>
#include <arm/mmu.h>
#include <util/atomic.h>

struct system_coproc {
	word id; // cr0
	word cr1; // cr1
	word process_id; // cr13
};

static struct system_coproc cp15;

static void op_cp15_reg_transfer(word ins, void *data)
{
	int opcode_1, opcode_2;
	int CRn, CRm;
	int Rd;
	int L;
	int cp_num;
	reg_t val;

	// decode the fields in the instruction
	CRm = BITS(ins, 3, 0);	
	opcode_2 = BITS_SHIFT(ins, 7, 5);
	cp_num = BITS_SHIFT(ins, 11, 8);
	Rd = BITS_SHIFT(ins, 15, 12);
	CRn = BITS_SHIFT(ins, 19, 16);
	L = BIT_SET(ins, 20);
	opcode_1 = BITS_SHIFT(ins, 23, 21);

	CPU_TRACE(5, "\t\top_cp15_reg_transfer: cp_num %d L %d Rd %d CRn %d CRm %d op1 %d op2 %d\n",
		cp_num, L, Rd, CRn, CRm, opcode_1, opcode_2);

#if COUNT_ARM_OPS
	inc_perf_counter(OP_COP_REG_TRANS);
#endif
	/* some coprocessors we dont care about */
	switch(CRn) {
		case 0: // id register
			if(L) {
				val = cp15.id;
				goto loadval;
			}
			goto done;
		case 1: // system control register
			if(L) {
				val = cp15.cr1;
				goto loadval;
			} else {
				word newval = get_reg(Rd);

				/* for now, we only support setting mmu flags */
				if((newval & MMU_FLAG_MASK ) != newval) {
					/* some other flags were being set, we can't deal */
					panic_cpu("unsupported bits in cp15.cr1 are being set (0x%x)\n", newval);
				}

				/* save our config */
				cp15.cr1 = newval;

				/* load the potentially new mmu config */
				mmu_set_flags(newval & MMU_FLAG_MASK);

				goto done;
			}
		case 2: // translation table base
			if(L) {
				val = mmu_get_register(MMU_TRANS_TABLE_REG);
				goto loadval;
			} else {
				mmu_set_register(MMU_TRANS_TABLE_REG, get_reg(Rd));
				goto done;
			}
		case 3: // domain access control
			if(L) {
				val = mmu_get_register(MMU_DOMAIN_ACCESS_CONTROL_REG);
				goto loadval;
			} else {
				mmu_set_register(MMU_DOMAIN_ACCESS_CONTROL_REG, get_reg(Rd));
				goto done;
			}
		case 4: // unpredictable
			goto donothing;
		case 5: // fault state register
			if(L) {
				val = mmu_get_register(MMU_FAULT_STATUS_REG);
				goto loadval;
			} else {
				mmu_set_register(MMU_FAULT_STATUS_REG, get_reg(Rd));
				goto done;
			}		
		case 6: // fault address register
			if(L) {
				val = mmu_get_register(MMU_FAULT_ADDRESS_REG);
				goto loadval;
			} else {
				mmu_set_register(MMU_FAULT_ADDRESS_REG, get_reg(Rd));
				goto done;
			}	
		case 7: // data cache registers, we dont emulate
			goto donothing;
		case 8: // tlb flush
			goto donothing;
		case 9: // cache lockdown
			goto donothing;
		case 10: // tlb lockdown
			goto donothing;
		case 11: 
		case 12: // unpredictable
			goto donothing;
		case 13: // process id register
			if(L) {
				val = cp15.process_id;
				goto loadval;
			} else {
				cp15.process_id = get_reg(Rd);
				goto done;
			}
		case 14: // reserved
			goto donothing;
		case 15: // test and debug
			goto donothing;
		default:
			goto donothing;
	}

loadval:
	if(L && Rd != 15)
		put_reg(Rd, val);
	goto done;

unsupported:
	panic_cpu("reading from or writing to unsupported cp15 control register!\n");

donothing:
	if(L && Rd != 15) {
		put_reg(Rd, 0);
	}

done:
	;
}

static void op_cp15_double_reg_transfer(word ins, void *data)
{
	int CRm;
	int Rn, Rd;
	int opcode;
	int cp_num;
	int L;

	// decode the fields in the instruction
	CRm = BITS(ins, 3, 0);
	opcode = BITS_SHIFT(ins, 7, 4);
	cp_num = BITS_SHIFT(ins, 11, 8);
	Rd = BITS_SHIFT(ins, 15, 12);
	Rn = BITS_SHIFT(ins, 19, 16);
	L = BIT_SET(ins, 20);

	CPU_TRACE(5, "\t\top_cp15_double_reg_transfer: cp_num %d L %d Rd %d CRn %d CRm %d opcode %d\n",
		cp_num, L, Rd, CRm, opcode);	

	panic_cpu("op_co_double_reg_transfer unimplemented\n");
	CPU_TRACE(1, "warning, ignoring coprocessor instruction\n");

#if COUNT_ARM_OPS
	inc_perf_counter(OP_COP_REG_TRANS);
#endif
}

static void op_cp15_data_processing(word ins, void *data)
{
	int opcode_1, opcode_2;
	int CRn, CRm;
	int CRd;
	int cp_num;

	// decode the fields in the instruction
	CRm = BITS(ins, 3, 0);	
	opcode_2 = BITS_SHIFT(ins, 7, 5);
	cp_num = BITS_SHIFT(ins, 11, 8);
	CRd = BITS_SHIFT(ins, 15, 12);
	CRn = BITS_SHIFT(ins, 19, 16);
	opcode_1 = BITS_SHIFT(ins, 23, 20);

	CPU_TRACE(5, "\t\top_cp15_data_processing: cp_num %d CRd %d CRn %d CRm %d op1 %d op2 %d\n",
		cp_num, CRd, CRn, CRm, opcode_1, opcode_2);

	panic_cpu("op_co_data_processing unimplemented\n");
	CPU_TRACE(1, "warning, ignoring coprocessor instruction\n");

#if COUNT_ARM_OPS
	inc_perf_counter(OP_COP_DATA_PROC);
#endif
}

static void op_cp15_load_store(word ins, void *data)
{
	int CRd;
	int Rn;
	int cp_num;
	int P, U, N, W, L;
	int offset_8;

	// decode the fields in the instruction
	offset_8 = BITS(ins, 7, 0);
	cp_num = BITS_SHIFT(ins, 11, 8);
	CRd = BITS_SHIFT(ins, 15, 12);
	Rn = BITS_SHIFT(ins, 19, 16);
	L = BIT_SET(ins, 20);
	W = BIT_SET(ins, 21);
	N = BIT_SET(ins, 22);
	U = BIT_SET(ins, 23);
	P = BIT_SET(ins, 24);

	CPU_TRACE(5, "\t\top_cp15_load_store: cp_num %d L %d W %d N %d U %d P %d Rn %d CRd %d offset_8 %d\n",
		cp_num, L, W, N, U, P, Rn, CRd, offset_8);

	panic_cpu("op_co_load_store unimplemented\n");
	CPU_TRACE(1, "warning, ignoring coprocessor instruction\n");

#if COUNT_ARM_OPS
	inc_perf_counter(OP_COP_LOAD_STORE);
#endif
}


void install_cp15(void)
{
	struct arm_coprocessor cp15_coproc;

	cp15_coproc.installed = 1;
	cp15_coproc.data = (void *)&cp15;

	cp15_coproc.reg_transfer = op_cp15_reg_transfer;
	cp15_coproc.double_reg_transfer = op_cp15_double_reg_transfer;
	cp15_coproc.data_processing = op_cp15_data_processing;
	cp15_coproc.load_store = op_cp15_load_store;

	install_coprocessor(15, &cp15_coproc);
}

