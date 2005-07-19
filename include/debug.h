#ifndef __DEBUG_H
#define __DEBUG_H

#include <stdarg.h>
#include <options.h> // for TRACE_XXX_LEVEL

void dprintf(const char *fmt, ...);

/* trace */
#define CPU_TRACE(level, x...) { if((level) <= TRACE_CPU_LEVEL) dprintf(x); } while(0)
#define UOP_TRACE(level, x...) { if((level) <= TRACE_UOP_LEVEL) dprintf(x); } while(0)
#define SYS_TRACE(level, x...) { if((level) <= TRACE_SYS_LEVEL) dprintf(x); } while(0)
#define MMU_TRACE(level, x...) { if((level) <= TRACE_MMU_LEVEL) dprintf(x); } while(0)

#endif

