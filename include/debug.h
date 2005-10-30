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
#ifndef __DEBUG_H
#define __DEBUG_H

#include <stdarg.h>
#include <options.h> // for TRACE_XXX_LEVEL

void dprintf(const char *fmt, ...);

#if DYNAMIC_TRACE_LEVELS
extern int TRACE_CPU_LEVEL;
extern int TRACE_UOP_LEVEL;
extern int TRACE_SYS_LEVEL;
extern int TRACE_MMU_LEVEL;
#endif

/* trace */
#define CPU_TRACE(level, x...) { if((level) <= TRACE_CPU_LEVEL) dprintf(x); } while(0)
#define UOP_TRACE(level, x...) { if((level) <= TRACE_UOP_LEVEL) dprintf(x); } while(0)
#define SYS_TRACE(level, x...) { if((level) <= TRACE_SYS_LEVEL) dprintf(x); } while(0)
#define MMU_TRACE(level, x...) { if((level) <= TRACE_MMU_LEVEL) dprintf(x); } while(0)

#endif

