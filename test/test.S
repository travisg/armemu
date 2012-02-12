.arm
.org 0

.section .text.boot
.globl _start
_start:
_vectors:
	b		reset
	b		undefined
	b		syscall
	b		prefetch_abort
	b		data_abort
	b		reserved
	b		irq
	b		fiq

test:
	b		test2

#if 0
	.skip	4096
#endif
test2:
#define EMIT4(ins...) ins; ins; ins; ins;
#define EMIT16(ins...) EMIT4(ins) EMIT4(ins) EMIT4(ins) EMIT4(ins)
#define EMIT64(ins...) EMIT16(ins) EMIT16(ins) EMIT16(ins) EMIT16(ins)
#define EMIT256(ins...) EMIT64(ins) EMIT64(ins) EMIT64(ins) EMIT64(ins)
//	EMIT256(nop)
//	EMIT256(nop)
//	EMIT256(nop)
//	EMIT256(nop)
//	EMIT256(nop)
//	EMIT256(eor r0, r0, r1)
//	EMIT256(add r0, r0, r1)
	b test2

//	EMIT256(add r0, r1, #1)
//	EMIT256(ldr		r0, =test)
foo:
//	ldr		r0, =foo
//	EMIT256(ldr		r0, =foo)
//	bx		r0	
	b		test
.ltorg

reset: /* arm, in svc mode */
//	b		test

	/* PLD instruction */
	pld		[r0]

	/* clz instruction */
	clz		r0, r1
	
	/* set up the svc stack */
	ldr		sp, =svc_stack_top

	/* set up the stack for irq, fiq, abort, undefined, and lastly system mode */
	mrs		r0, cpsr
	bic		r0, r0, #0x1f

	orr		r1, r0, #0x12 // irq
	msr		cpsr_c, r1
	ldr		sp, =irq_stack_top

	orr		r1, r0, #0x11 // fiq
	msr		cpsr_c, r1
	ldr		sp, =fiq_stack_top
		
	orr		r1, r0, #0x17 // abort
	msr		cpsr_c, r1
	ldr		sp, =abt_stack_top

	orr		r1, r0, #0x1b // undefined
	msr		cpsr_c, r1
	ldr		sp, =und_stack_top

	orr		r1, r0, #0x1f // system
	msr		cpsr_c, r1
	ldr		sp, =sys_stack_top
	// leave the cpu in system mode

	/* make sure the coprocessor stuff decodes properly */
	bl		coprocessor_test

0:
	bl		main
	b		0b

undefined:
	stmfd	sp!,{r0-r12,lr}

	ldr		r0, =undefined_handler
	mov		lr, pc
	bx		r0

	ldmfd	sp!,{r0-r12,lr}
	movs	pc,lr // instruction *after* the undefined instruction

syscall:
	stmfd	sp!,{r0-r12,lr}

	ldr		r0, =swi_handler
	mov		lr, pc
	bx		r0

	ldmfd	sp!,{r0-r12,lr}
	movs	pc,lr

prefetch_abort:
	stmfd	sp!,{r0-r12,lr}

	ldr		r0, =prefetch_abort_handler
	mov		lr, pc
	bx		r0

	ldmfd	sp!,{r0-r12,lr}
	subs	pc,lr,#4	// restart the instruction that generated the fault

data_abort:
	stmfd	sp!,{r0-r12,lr}

	ldr		r0, =data_abort_handler
	mov		lr, pc
	bx		r0

	ldmfd	sp!,{r0-r12,lr}
	subs	pc,lr,#8	// restart the instruction that generated the fault

reserved:
	b	.

irq:
	stmfd	sp!,{r0-r12,lr}

	ldr		r0, =irq_handler
	mov		lr, pc
	bx		r0

	ldmfd	sp!,{r0-r12,lr}
	subs	pc,lr,#4

fiq:
	stmfd	sp!,{r0-r7,lr}

	ldr		r0, =fiq_handler
	mov		lr, pc
	bx		r0

	ldmfd	sp!,{r0-r7,lr}
	subs	pc,lr,#4

/* misc assembly instructions */
.global arm_enable_ints
arm_enable_ints:
	/* enable interrupts */
	mrs		r0, cpsr
	bic		r0, r0, #(1<<7)
	msr		cpsr, r0
	bx		lr

.global coprocessor_test
coprocessor_test:
	/* hit some coprocessor instructions */
//	mov		r12, #0
//	mrc		p15, 1, r12, cr2, cr3, 2 /* coprocessor 5, opcodes 1/2, CRn 2, CRm 3 */

	/* floating point stuff which should hit the coprocessor */
//	fmacs	s0, s1, s2
//	fmacd	d0, d1, d2
	bx		lr

/* coprocessor 15 routines for arm926es cores */
.arm
.text

	.globl	arm_flush_invalidate_d_cache
arm_flush_invalidate_d_cache:
	mrc		p15, 0x0, r15, c7, c14, 3	/* test, clean, and invalidate Dcache */
	bne 	arm_flush_invalidate_d_cache

	mov		r0, #0
	mcr		p15, 0, r0, c7, c10, 4		/* drain write buffer */
	bx		lr

	.globl	arm_flush_d_cache
arm_flush_d_cache:
	mrc		p15, 0x0, r15, c7, c10, 3	/* test and clean Dcache */
	bne 	arm_flush_d_cache

	mov		r0, #0
	mcr		p15, 0, r0, c7, c10, 4		/* drain write buffer */
	bx		lr

	.globl	arm_invalidate_i_cache
arm_invalidate_i_cache:
	mov 	r0, #0
	mcr 	p15, 0x0, r0, c7, c5, 0x0 /* invalidate I-cache */
	bx		lr

	.globl	arm_invalidate_d_cache
arm_invalidate_d_cache:
	mov 	r0, #0
	mcr 	p15, 0x0, r0, c7, c6, 0x0 /* invalidate D-cache */
	bx		lr

	.globl	arm_drain_write_buffer
arm_drain_write_buffer:
	mov 	r0, #0
	mcr 	p15, 0, r0, c7, c10, 4		/* drain write buffer */
	bx		lr

	.globl	arm_tlb_flush
arm_tlb_flush:
	mov		r0, #0
	mcr 	p15, 0, r0, c7, c10, 4		/* drain write buffer */
	mcr 	p15, 0, r0, c8, c7, 0		/* globally flush TLB */
	bx		lr

	.globl	read_cpu_id
read_cpu_id:
	mrc		p15, 0x0, r0, c0, c0, 0x0
	bx		lr

	.globl	read_cir
read_cir:
	mrc		p15, 0x0, r0, c0, c0, 0x1
	bx		lr

	.globl	read_cp15
read_cp15:
	mrc		p15, 0x0, r0, c1, c0, 0x0
	bx		lr

	.globl	write_cp15
write_cp15:
	mcr		p15, 0x0, r0, c1, c0, 0x0
	nop
	nop
	nop
	bx		lr

	.globl read_trans_table_base
read_trans_table_base:
	mrc		p15, 0x0, r0, c2, c0, 0x0
	bx		lr

	.globl write_trans_table_base
write_trans_table_base:
	mcr		p15, 0x0, r0, c2, c0, 0x0
	bx		lr
	
	.globl	write_domain_control_reg
write_domain_control_reg:
	mcr		p15, 0x0, r0, c3, c0, 0x0
	bx		lr

	.globl	read_fault_sr
read_fault_sr:
	mrc		p15, 0x0, r0, c5, c0, 0x0
	bx		lr
	
	.globl	read_fault_ar
read_fault_ar:
	mrc		p15, 0x0, r0, c6, c0, 0x0
	bx		lr

.data

.align 4
sys_stack:
	.skip 4096
sys_stack_top:

irq_stack:
	.skip 4096
irq_stack_top:

fiq_stack:
	.skip 4096
fiq_stack_top:

abt_stack:
	.skip 4096
abt_stack_top:

und_stack:
	.skip 4096
und_stack_top:

svc_stack:
	.skip 4096
svc_stack_top:
