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
#ifndef __ENDIAN_H
#define __ENDIAN_H

#include <sys/param.h>

#if __POWERPC__
#include <ppc_intrinsics.h>
#endif

#ifndef BYTE_ORDER
#error "need to get the BYTE_ORDER define from somewhere"
#endif

// are we big endian?
#if BYTE_ORDER == BIG_ENDIAN
#define NEEDS_SWAP 1
#endif

// we'll use the powerpc's builtin byte swapping instructions
#if __POWERPC__
#undef NEEDS_SWAP
#define NEEDS_SWAP 0
#endif

// define a macro that conditionally swaps
#define SWAP_WORD(x) \
    (((x) >> 24) | (((x) >> 8) & 0xff00) | (((x) << 8) & 0xff0000) | ((x) << 24))
#define SWAP_HALFWORD(x) \
    (((x) >> 8) | ((x) << 8))

#if NEEDS_SWAP
#define SWAPIT_WORD(x) SWAP_WORD(x)
#define SWAPIT_HALFWORD(x) SWAP_HALFWORD(x)
#else
#define SWAPIT_WORD(x) (x)
#define SWAPIT_HALFWORD(x) (x)
#endif

// some memory access macros
#if __POWERPC__
#define READ_MEM_WORD(ptr)      __lwbrx((word *)(ptr), 0)
#define READ_MEM_HALFWORD(ptr)  __lhbrx((halfword *)(ptr), 0)
#define READ_MEM_BYTE(ptr)      (*(byte *)(ptr))
#define WRITE_MEM_WORD(ptr, data)   __stwbrx(data, (word *)(ptr), 0)
#define WRITE_MEM_HALFWORD(ptr, data)   __sthbrx(data, (halfword *)(ptr), 0)
#define WRITE_MEM_BYTE(ptr, data)   (*(byte *)(ptr) = (data))
#else
#define READ_MEM_WORD(ptr)      SWAPIT_WORD(*(word *)(ptr))
#define READ_MEM_HALFWORD(ptr)  SWAPIT_HALFWORD(*(halfword *)(ptr))
#define READ_MEM_BYTE(ptr)      (*(byte *)(ptr))
#define WRITE_MEM_WORD(ptr, data)   (*(word *)(ptr) = SWAPIT_WORD(data))
#define WRITE_MEM_HALFWORD(ptr, data)   (*(halfword *)(ptr) = SWAPIT_HALFWORD(data))
#define WRITE_MEM_BYTE(ptr, data)   (*(byte *)(ptr) = (data))
#endif


#endif
