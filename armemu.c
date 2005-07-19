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
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <getopt.h>
#include <sys/sys.h>

#include <SDL/SDL.h>
#include <SDL/SDL_thread.h>

static const char *opt_binary = NULL;
static const char *opt_romfile = NULL;
static int opt_cycles = 0;

static void usage(int argc, char **argv)
{
	printf("usage: %s [-b binary] [-r romfile]\n", argv[0]);

	exit(1);
}

static int init_sdl(void)
{
	atexit(SDL_Quit);

	SDL_Init(SDL_INIT_VIDEO|SDL_INIT_TIMER);
	SDL_EnableUNICODE(1);

	return 0;
}

int main(int argc, char **argv)
{
	for(;;) {
		int c;
		int option_index = 0;

		static struct option long_options[] = {
			{"binary", 1, 0, 'b'},
			{"numcycles", 1, 0, 'n'},
			{"rom", 1, 0, 'r'},
		};
		
		c = getopt_long(argc, argv, "b:n:r:", long_options, &option_index);
		if(c == -1)
			break;

		switch(c) {
			case 'b':
				printf("load binary option: '%s'\n", optarg);
				opt_binary = optarg;
				break;
			case 'n':
				opt_cycles = atoi(optarg);
				printf("cycle count option: '%d'\n", opt_cycles);
				break;
			case 'r':
				printf("load romfile option: '%s'\n", optarg);
				opt_romfile = optarg;
				break;
			default:
				usage(argc, argv);
				break;
		}
	}

	// bring up the SDL system
	init_sdl();

	// initialize the system
	initialize_system(opt_binary, opt_romfile);

	// start the system, should spawn a cpu thread
	system_start(opt_cycles);

	// run the SDL message loop
	system_message_loop();

	return 0;
}

