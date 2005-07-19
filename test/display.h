#ifndef __DISPLAY_H
#define __DISPLAY_H

// display
void clear_display(unsigned int color);
int test_display(void);
void draw_pixel(int x, int y, unsigned int color);
void fill_rect(int x, int y, int w, int h, unsigned int color);
void copy_rect(int x, int y, int w, int h, int x2, int y2);
unsigned int *get_fb(void);

// screen specs
#define SCREEN_X 640
#define SCREEN_Y 480
#define SCREEN_BITDEPTH 32
#define SCREEN_FB_SIZE (4*1024*1024)

// there is more buffer available for blitting to/from
#define SCREEN_BUF_Y (SCREEN_FB_SIZE / SCREEN_X / (SCREEN_BITDEPTH / 8)) 

#endif
