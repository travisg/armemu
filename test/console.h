#ifndef __CONSOLE_H
#define __CONSOLE_H

enum {
    KEY_MOD_UP = 0x80000000
};

// keyboard routines
int read_keyboard(unsigned int *key);
void keyboard_int_handler(void);

#endif
