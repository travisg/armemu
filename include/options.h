#ifndef __OPTIONS_H
#define __OPTIONS_H

#define TRACE_CPU_LEVEL 1
#define TRACE_UOP_LEVEL 1
#define TRACE_SYS_LEVEL 1
#define TRACE_MMU_LEVEL 1

#define COUNT_CYCLES 	0 // should we try to accurately count cycles
#define COUNT_ARM_OPS	0
#define COUNT_UOPS		0
#define COUNT_ARITH_UOPS 0

// compiler hints
#define likely(x)   __builtin_expect(!!(x), 1)
#define unlikely(x) __builtin_expect(!!(x), 0)
#define __ALWAYS_INLINE __attribute__((always_inline))

// systemwide asserts
#if 0
#define ASSERT(x) \
	if(unlikely(!(x))) panic_cpu("assert failed at %s:%d\n", __FUNCTION__, __LINE__);
#else
#define ASSERT(x)
#endif

#endif
