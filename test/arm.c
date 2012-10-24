
#define ARRAY_SIZE 1024
static int bar_buf[ARRAY_SIZE];
static int baz_buf[ARRAY_SIZE];
int mul = 87;

/* should hit load/store pretty heavily */
void arm_rotate_right_array(int *outarray, int *inarray, int count, int rotate)
{
    int i;
    int targetindex;

    for (i=0, targetindex = rotate; i < count - 1; i++, targetindex++) {
        if (targetindex >= count)
            targetindex -= count;
        outarray[targetindex] = inarray[i];
    }
}

int test_arm_ptr_routine(int a)
{
    return a+1;
}

int armfunc(int a)
{
    int i;
    int foo = 0x78324234;
    int *baz = baz_buf;
    int *bar = bar_buf;
    int (*func)(int) = &test_arm_ptr_routine;

    for (i=0; i < ARRAY_SIZE; i++) {
        if (i > 0)
            foo = bar[i-1];
        foo += i + a;
        foo *= mul;
        foo -= 27;
        foo |= foo ^ (foo >> 2);
        foo &= 0xffeffeff;
        if (foo & 8)
            foo <<= 1;
        if (foo == a)
            foo = 0x9127345;
        foo >>= a;
        foo = ~foo;
        foo = func(foo);
        bar[i] = foo;
    }

    arm_rotate_right_array(baz, bar, ARRAY_SIZE, 5);

    return foo > 87;
}

__attribute__((naked)) void armasm(void)
{
    asm(
        /* PLD instruction */
        "pld		[r0];"

        /* clz instruction */
        "clz		r0, r1;"

        /* ubfx */
        "ubfx	r0, r1, #0, #1;"
        "ubfx	r0, r1, #1, #1;"
        "ubfx	r0, r1, #30, #2;"
        "sbfx	r0, r1, #0, #1;"
        "sbfx	r0, r1, #30, #2;"
        "sbfx	r0, r1, #1, #31;"

        "movw	r2, #0x5678;"
        "movt	r2, #0x1234;"

        "bx	lr;"
    );
}

