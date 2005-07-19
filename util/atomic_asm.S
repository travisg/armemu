/* atomic routines per host processor */

.text

/* Some host architectures require underscores in front of all of the symbols */
/*
 * Can't combine the two macros because some assemblers require .globl directives
 * to be on their own line
 */
#if ASM_LEADING_UNDERSCORES
#define GLOBAL(x) .globl _##x
#define FUNCTION(x) _##x
#elif ASM_LEADING_DOTS
#define GLOBAL(x) .globl .##x
#define FUNCTION(x) .##x
#else
#define GLOBAL(x) .globl x
#define FUNCTION(x) x
#endif

//#define FUNCTION(x) .globl x; .type x,@function; x

#if __i386__ || __I386__

/* int atomic_add(int *val, int incr) */
GLOBAL(atomic_add)
FUNCTION(atomic_add):
	movl	4(%esp),%edx
	movl	8(%esp),%eax
	lock
	xaddl	%eax,(%edx)
	ret

/* int atomic_and(int *val, int incr) */
GLOBAL(atomic_and)
FUNCTION(atomic_and):
	movl	4(%esp),%edx

1:
	movl	8(%esp),%ecx
	movl	(%edx),%eax
	andl	%eax,%ecx

	lock
	cmpxchgl	%ecx,(%edx)

	jnz		1b

	ret

/* int atomic_or(int *val, int incr) */
GLOBAL(atomic_or)
FUNCTION(atomic_or):
	movl	4(%esp),%edx

1:
	movl	8(%esp),%ecx
	movl	(%edx),%eax
	orl		%eax,%ecx

	lock
	cmpxchgl	%ecx,(%edx)

	jnz		1b

	ret

/* int atomic_set(int *val, int set_to) */
GLOBAL(atomic_set)
FUNCTION(atomic_set):
	movl	4(%esp),%edx
	movl	8(%esp),%eax
	xchg	%eax,(%edx)
	ret

/* int test_and_set(int *val, int set_to, int test_val) */
GLOBAL(test_and_set)
FUNCTION(test_and_set):
	movl	4(%esp),%edx
	movl	8(%esp),%ecx
	movl	12(%esp),%eax

	lock
	cmpxchgl	%ecx,(%edx)

	ret
#endif

#if __amd64__

/* int atomic_add(int *val, int incr) */
GLOBAL(atomic_add)
FUNCTION(atomic_add):
	movl		%esi,%eax		/* incr -> eax */
	lock
	xaddl	%eax,(%esi)		/* atomically add incr to *val */
							/* old value is in eax */
	ret

/* int atomic_and(int *val, int incr) */
GLOBAL(atomic_and)
FUNCTION(atomic_and):
1:
	movl		(%edi),%eax		/* *val -> temp2 */
	movl		%esi,%ecx		/* incr -> temp */
	andl		%eax,%ecx		/* temp &= temp2 */

	lock
	cmpxchgl	%ecx,(%edi) /*,%eax*/	/* if temp2 is same as *val, temp -> *val */
							/* old value is in eax */

	jnz		1b			/* if it didn't succeed, try again */
	ret

/* int atomic_or(int *val, int incr) */
GLOBAL(atomic_or)
FUNCTION(atomic_or):
1:
	movl		(%edi),%eax		/* *val -> temp2 */
	movl		%esi,%ecx		/* incr -> temp */
	orl		%eax,%ecx		/* temp |= temp2 */

	lock
	cmpxchgl	%ecx,(%edi) /*,%eax*/	/* if temp2 is same as *val, temp -> *val */
							/* old value is in eax */

	jnz			1b			/* if it didn't succeed, try again */
	ret

/* int atomic_set(int *val, int set_to) */
GLOBAL(atomic_set)
FUNCTION(atomic_set):
	movl		%esi,%eax		/* set_to -> eax */
	xchg		%eax,(%edi)		/* *val += set_to */
							/* old value is in eax */
	ret

/* int test_and_set(int *val, int set_to, int test_val) */
GLOBAL(test_and_set)
FUNCTION(test_and_set):
	movl		%edx,%eax		/* test_val -> eax */

	lock
	cmpxchgl	%esi,(%edi) /*,%eax*/	/* if test_val is same as *val, set_to -> *val */
							/* old value is in eax */
	ret
#endif

#if __ppc__ || __PPC__

/* int atomic_add(volatile int *val, int incr) */
GLOBAL(atomic_add)
FUNCTION(atomic_add):
1:
	isync
	lwarx	r5, 0, r3					// load & reserve
	add		r0, r4, r5					// do the add
	stwcx.	r0, 0, r3					// store the new value
	bne-	1b							// did we lose reservation? if so, repeat
	mr		r3, r5						// return the old value
	isync
	blr

/* int atomic_and(volatile int *val, int incr) */
GLOBAL(atomic_and)
FUNCTION(atomic_and):
1:
	isync
	lwarx	r5, 0, r3					// load & reserve
	and		r0, r4, r5					// do the and
	stwcx.	r0, 0, r3					// store the new value
	bne-	1b							// did we lose reservation? if so, repeat
	mr		r3, r5						// return the old value
	isync
	blr

/* int atomic_or(volatile int *val, int incr) */
GLOBAL(atomic_or)
FUNCTION(atomic_or):
1:
	isync
	lwarx	r5, 0, r3					// load & reserve
	or		r0, r4, r5					// do the or
	stwcx.	r0, 0, r3					// store the new value
	bne-	1b							// did we lose reservation? if so, repeat
	mr		r3, r5						// return the old value
	isync
	blr

/* int atomic_set(volatile int *val, int set_to) */
GLOBAL(atomic_set)
FUNCTION(atomic_set):
1:
	isync
	lwarx	r5, 0, r3					// load & reserve
	stwcx.	r4, 0, r3					// store the new value
	bne-	1b							// did we lose reservation? if so, repeat
	mr		r3, r5						// return the old value
	blr

/* int test_and_set(int *val, int set_to, int test_val) */
GLOBAL(test_and_set)
FUNCTION(test_and_set):
1:
	isync
	lwarx	r1, 0, r3					// load & reserve
	cmpw	r1, r5						// see if the value == test_val
	bne-	2f							// if not, bail

	stwcx.	r4, 0, r3					// store the new value
	bne-	1b							// did we lose reservation? if so, repeat
2:
	mr		r3, r1						// return what was there (success or failure)
	blr
#endif

#if __sparc__ || __SPARC__
#error implement SPARC atomic_* ops
#endif



