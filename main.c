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

#include <config.h>

#include <sys/ioctl.h>
#include <termios.h>

static struct termios oldstdin;
static struct termios oldstdout;

static void usage(int argc, char **argv)
{
	fprintf(stderr, "usage: %s [-b binary] [-c cpu type] [-r romfile] [-n cycle count]\n", argv[0]);

	exit(1);
}

static int init_sdl(void)
{
	atexit(SDL_Quit);

	return SDL_Init(SDL_INIT_TIMER);
}

static void resetconsole(void)
{
	tcsetattr(0, TCSANOW, &oldstdin);
	tcsetattr(1, TCSANOW, &oldstdout);
}

static void setconsole(void)
{
	struct termios t;

	tcgetattr(0, &oldstdin);
	tcgetattr(1, &oldstdout);

	atexit(&resetconsole);

	t = oldstdin;
	t.c_lflag = ISIG; // no input processing
	// Don't interpret various control characters, pass them through instead
	t.c_cc[VINTR] = t.c_cc[VQUIT] = t.c_cc[VSUSP] = '\0';
	t.c_cc[VMIN]  = 0; // nonblocking read
	t.c_cc[VTIME] = 0; // nonblocking read
	tcsetattr(0, TCSANOW, &t);

	t = oldstdout;
	t.c_lflag = ISIG; // no output processing
	// Don't interpret various control characters, pass them through instead
	t.c_cc[VINTR] = t.c_cc[VQUIT] = t.c_cc[VSUSP] = '\0';
	tcsetattr(1, TCSANOW, &t);
}


int main(int argc, char **argv)
{
	// load the configuration of the emulator from the command line and config file
	load_config(argc, argv);

	// read in any overriding configuration from the command line
	for(;;) {
		int c;
		int option_index = 0;

		static struct option long_options[] = {
			{"rom", 1, 0, 'r'},
			{"cpu", 1, 0, 'c'},
			{0, 0, 0, 0},
		};
		
		c = getopt_long(argc, argv, "r:c:", long_options, &option_index);
		if(c == -1)
			break;

		switch(c) {
			case 'r':
				printf("load rom option: '%s'\n", optarg);
				add_config_key("rom", "file", optarg);
				break;
			case 'c':
				printf("cpu core option: '%s'\n", optarg);
				add_config_key("cpu", "core", optarg);
				break;
			default:
				usage(argc, argv);
				break;
		}
	}

	setconsole();

	// bring up the SDL system
	if (init_sdl() < 0) {
		fprintf(stderr, "error initializing sdl. aborting...\n");
		return 1;
	}

	// initialize the system
	if (initialize_system() < 0) {
		fprintf(stderr, "failed to initialize system, bailing\n");
		return 1;
	}

	// start the system, should spawn a cpu thread
	system_start();

	// run the SDL message loop
	system_message_loop();

	return 0;
}

