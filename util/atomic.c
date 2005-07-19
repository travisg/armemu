#include <util/atomic.h>

#if __i386__ || __I386__

/* defined in atomic_asm.S */

#endif

#if __ppc__ || __PPC__

/* defined in atomic_asm.S */

#endif

#if __sparc__ || __SPARC__
#error implement SPARC atomic_* ops
#endif

