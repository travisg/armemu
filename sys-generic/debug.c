#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <sys/types.h>

#include <debug.h>
#include <config.h>
#include <arm/arm.h>
#include <sys/sys.h>
#include "sys_p.h"

struct sys_debug {
	armaddr_t memory_dump_addr;
	unsigned int memory_dump_len;
};

static struct sys_debug debug;

static void dump_memory_byte(armaddr_t address, unsigned int len)
{
	int i = 0;
	while(len > 0) {
		byte data = sys_read_mem_byte(address);

		if((i % 16 == 0)) {
			if(i != 0)
				printf("\n");
			printf("0x%08x: ", address);
		}

		printf("%02x ", data);

		i++;
		len--;
		address++;
	}
	printf("\n");
}

static void dump_memory_halfword(armaddr_t address, unsigned int len)
{
	int i = 0;

	while(len > 0) {
		halfword data = sys_read_mem_halfword(address);

		if((i % 16 == 0)) {
			if(i != 0)
				printf("\n");
			printf("0x%08x: ", address);
		}

		printf("%04x ", data);

		i++;
		len--;
		address += 2;
	}
	printf("\n");
}

static void dump_memory_word(armaddr_t address, unsigned int len)
{
	int i = 0;

	while(len > 0) {
		word data = sys_read_mem_word(address);

		if((i % 16 == 0)) {
			if(i != 0)
				printf("\n");
			printf("0x%08x: ", address);
		}

		printf("%08x ", data);

		i++;
		len--;
		address += 4;
	}
	printf("\n");
}


static word debug_get_put(armaddr_t address, word data, int size, int put)
{
    char x;
        
    if(put) {
        switch(address){
        case DEBUG_STDOUT: 
            x = data;
            write(1, &x, 1);
            return 0;
        case DEBUG_REGDUMP:
            dump_registers();
            return 0;
		case DEBUG_HALT:
			panic_cpu("debug halt\n");
			return 0;
		case DEBUG_MEMDUMPADDR:
			debug.memory_dump_addr = data;
			return 0;
		case DEBUG_MEMDUMPLEN:
			debug.memory_dump_len = data;
			return 0;
		case DEBUG_MEMDUMP_BYTE:
			dump_memory_byte(debug.memory_dump_addr, debug.memory_dump_len);
			return 0;
		case DEBUG_MEMDUMP_HALFWORD:
			dump_memory_halfword(debug.memory_dump_addr, debug.memory_dump_len);
			return 0;
		case DEBUG_MEMDUMP_WORD:
			dump_memory_word(debug.memory_dump_addr, debug.memory_dump_len);
			return 0;
#if DYNAMIC_TRACE_LEVELS
		case DEBUG_SET_TRACELEVEL_CPU:
			TRACE_CPU_LEVEL = data;
			return 0;
		case DEBUG_SET_TRACELEVEL_UOP:
			TRACE_UOP_LEVEL = data;
			return 0;
		case DEBUG_SET_TRACELEVEL_SYS:
			TRACE_SYS_LEVEL = data;
			return 0;
		case DEBUG_SET_TRACELEVEL_MMU:
			TRACE_MMU_LEVEL = data;
			return 0;
#endif			
        default:
            return 0;
        }
    } else {
        switch(address) {
        case DEBUG_STDIN: 
            if(read(0, &x, 1) == 1){
                return x;
            } else {
                return -1;
            }
#if COUNT_CYCLES
		case DEBUG_CYCLE_COUNT:
			return get_cycle_count();
#endif
		case DEBUG_INS_COUNT:
			return get_instruction_count();
        default:
            return 0;
        }
    }
}

int initialize_debug(void)
{
    install_mem_handler(DEBUG_REGS_BASE, DEBUG_REGS_SIZE,
                        debug_get_put, NULL);

	memset(&debug, 0, sizeof(debug));
    return 0;
}

