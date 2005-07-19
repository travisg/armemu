#ifndef __ATOMIC_H
#define __ATOMIC_H

int atomic_add(volatile int *val, int incr);
int atomic_and(volatile int *val, int incr);
int atomic_or(volatile int *val, int incr);
int atomic_set(volatile int *val, int set_to);
int test_and_set(volatile int *val, int set_to, int test_val);

#endif

