#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <sys/types.h>

#include <config.h>
#include <arm/arm.h>
#include <sys/sys.h>
#include "sys_p.h"

static word debug_get_put(armaddr_t address, word data, int size, int put)
{
    char x;
    address &= 0xffffff;
        
    if(put) {
        switch(address){
        case 0: 
            x = data;
            write(1, &x, 1);
            return 0;
        case 4:
            dump_registers();
            return 0;
		case 8:
			panic_cpu("debug halt\n");
			return 0;
        default:
            return 0;
        }
    } else {
        switch(address) {
        case 0: 
            if(read(0, &x, 1) == 1){
                return x;
            } else {
                return -1;
            }
        default:
            return 0;
        }
    }
}

int initialize_debug(void)
{
    install_mem_handler(DEBUG_REGS_BASE, DEBUG_REGS_SIZE,
                        debug_get_put, NULL);
    return 0;
}

