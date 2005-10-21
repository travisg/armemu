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
