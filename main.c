/*
 * newtest.c
 *
 * Copyright (c) 2014 Jeremy Garff <jer @ jers.net>
 *
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without modification, are permitted
 * provided that the following conditions are met:
 *
 *     1.  Redistributions of source code must retain the above copyright notice, this list of
 *         conditions and the following disclaimer.
 *     2.  Redistributions in binary form must reproduce the above copyright notice, this list
 *         of conditions and the following disclaimer in the documentation and/or other materials
 *         provided with the distribution.
 *     3.  Neither the name of the owner nor the names of its contributors may be used to endorse
 *         or promote products derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND
 * FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA,
 * OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT
 * OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 */


static char VERSION[] = "testing...";

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <signal.h>
#include <stdarg.h>
#include <getopt.h>


#include "clk.h"
#include "gpio.h"
#include "dma.h"
#include "pwm.h"
#include "version.h"

#include "ws2811.h"


#define ARRAY_SIZE(stuff)                        (sizeof(stuff) / sizeof(stuff[0]))

// defaults for cmdline options
#define TARGET_FREQ                              WS2811_TARGET_FREQ
#define GPIO_PIN                                 18
#define DMA                                      5
#define STRIP_TYPE				                 WS2811_STRIP_RGB		// WS2812/SK6812RGB integrated chip+leds
//#define STRIP_TYPE				             WS2811_STRIP_GBR		// WS2812/SK6812RGB integrated chip+leds
//#define STRIP_TYPE				             SK6812_STRIP_RGBW		// SK6812RGBW (NOT SK6812RGB)

#define WIDTH                                    12
#define HEIGHT                                   12
#define LED_COUNT                                (WIDTH * HEIGHT)

#define COL_WHITE								0x00FFFFFF
#define COL_RED									0x0000FF00
#define COL_BLUE								0x000000FF
#define COL_GREEN								0x00FF0000
#define COL_YELLOW								0x00FFFF00
#define COL_PURPLE								0x0000FFFF
#define COL_CYAN								0x00FF00FF

int width = WIDTH;
int height = HEIGHT;
int led_count = LED_COUNT;

int clear_on_exit = 0;

ws2811_t ledstring =
{
    .freq = TARGET_FREQ,
    .dmanum = DMA,
    .channel =
    {
        [0] =
        {
            .gpionum = GPIO_PIN,
            .count = LED_COUNT,
            .invert = 0,
            .brightness = 255,
            .strip_type = STRIP_TYPE,
        },
        [1] =
        {
            .gpionum = 0,
            .count = 0,
            .invert = 0,
            .brightness = 0,
        },
    },
};

ws2811_led_t *matrix;
//ws2811_led_t *adj_matrix;

static uint8_t running = 1;

const double PI=3.1415926535897932384650288;

double sin(double x){
  double sign=1;
  if (x<0){
    sign=-1.0;
    x=-x;
  }
  if (x>360) x -= ((int)(x/360))*360;
  x*=PI/180.0;
  double res=0;
  double term=x;
  int k=1;
  while (res+term!=res){
    res+=term;
    k+=2;
    term*=-x*x/k/(k-1);
  }

  return sign*res;
}

int dotspos[] = { 0, 1, 2, 3, 4, 5, 6, 7 };
ws2811_led_t dotcolors[] =
{
    0x00200000,  // red - REAL GREEN
    0x00201000,  // orange - REAL LIME
    0x00202000,  // yellow - REAL YELLOW
    0x00002000,  // green - REAL READ
    0x00002020,  // lightblue - REAL PURPLE
    0x00000020,  // blue - REAL BLUE
    0x00100010,  // purple - REAL CYAN
    0x00200010,  // pink - REAL AQUA
};

ws2811_led_t dotcolors_rgbw[] =
{
    0x00200000,  // red
    0x10200000,  // red + W
    0x00002000,  // green
    0x10002000,  // green + W
    0x00000020,  // blue
    0x10000020,  // blue + W
    0x00101010,  // white
    0x10101010,  // white + W

};

ws2811_led_t fullcolors[] = {
	0x00FF0000, // GREEN
	0x0000FF00, // RED
	0x000000FF, // BLUE
	0x00000000, // BLACK
	0x00FFFFFF, // WHITE
	
};

void matrix_clear(void)
{
    int x, y;

    for (y = 0; y < (height ); y++)
    {
        for (x = 0; x < width; x++)
        {
            matrix[y * width + x] = 0;
        }
    }
}

void matrix_render_oud(void)
{
    int x, y;

    for (x = 0; x < width; x++)
    {
        for (y = 0; y < height; y++)
        {
            ledstring.channel[0].leds[(y * width) + x] = matrix[y * width + x];
            
            
            
        }
    }
}

void matrix_render(void)
{
	int col = 1, row = 0;
	int i = 0;
	while ( row < height ) {
		ledstring.channel[0].leds[(width-col)*height+row] = matrix[i];
		ledstring.channel[0].leds[(width-col)*height-row-1] = matrix[i+1]; 
		col += 2;
		if (col > width) {
			row++;
			col = 1;
		}
		i += 2;
	}
	ws2811_return_t ret;
	if ((ret = ws2811_render(&ledstring)) != WS2811_SUCCESS) {
		fprintf(stderr, "ws2811_render failed: %s\n", ws2811_get_return_t_str(ret));
	}
}

int render_image_skull(float anim) {
	if ((width < 12)||(height < 12)) {
		printf ("ERROR: Matrix does not fit image_skull");
		return -1;
	}
	if (anim < 0) {
		anim = (-1)*anim;
	}
	
	int i = 0;
	int j = 0;
	while (j < width) {
		matrix[j] = 0;
		j++;
	}
		
	i++;
	// 1
	matrix[(i*width)+0] = 0;
	matrix[(i*width)+1] = 0;
	matrix[(i*width)+2] = 0;
	matrix[(i*width)+3] = 0;
	matrix[(i*width)+4] = COL_WHITE;
	matrix[(i*width)+5] = COL_WHITE;
	matrix[(i*width)+6] = COL_WHITE;
	matrix[(i*width)+7] = COL_WHITE;
	matrix[(i*width)+8] = 0;
	matrix[(i*width)+9] = 0;
	matrix[(i*width)+10] = 0;
	matrix[(i*width)+11] = 0;
	i++;
	// 2
	matrix[(i*width)+0] = 0;
	matrix[(i*width)+1] = 0;
	matrix[(i*width)+2] = 0;
	matrix[(i*width)+3] = COL_WHITE;
	matrix[(i*width)+4] = COL_WHITE;
	matrix[(i*width)+5] = COL_WHITE;
	matrix[(i*width)+6] = COL_WHITE;
	matrix[(i*width)+7] = COL_WHITE;
	matrix[(i*width)+8] = COL_WHITE;
	matrix[(i*width)+9] = 0;
	matrix[(i*width)+10] = 0;
	matrix[(i*width)+11] = 0;
	i++;
	// 3
	matrix[(i*width)+0] = 0;
	matrix[(i*width)+1] = 0;
	matrix[(i*width)+2] = COL_WHITE;
	matrix[(i*width)+3] = COL_WHITE;
	matrix[(i*width)+4] = COL_WHITE;
	matrix[(i*width)+5] = COL_WHITE;
	matrix[(i*width)+6] = COL_WHITE;
	matrix[(i*width)+7] = COL_WHITE;
	matrix[(i*width)+8] = COL_WHITE;
	matrix[(i*width)+9] = COL_WHITE;
	matrix[(i*width)+10] = 0;
	matrix[(i*width)+11] = 0;
	i++;
	// 4 
	matrix[(i*width)+0] = 0;
	matrix[(i*width)+1] = 0;
	matrix[(i*width)+2] = COL_WHITE;
	matrix[(i*width)+3] = COL_WHITE;
	matrix[(i*width)+4] = COL_WHITE;
	matrix[(i*width)+5] = COL_WHITE;
	matrix[(i*width)+6] = COL_WHITE;
	matrix[(i*width)+7] = COL_WHITE;
	matrix[(i*width)+8] = COL_WHITE;
	matrix[(i*width)+9] = COL_WHITE;
	matrix[(i*width)+10] = 0;
	matrix[(i*width)+11] = 0;
	i++;
	// 5
	matrix[(i*width)+0] = 0;
	matrix[(i*width)+1] = 0;
	matrix[(i*width)+2] = COL_WHITE;
	matrix[(i*width)+3] = COL_WHITE;
	matrix[(i*width)+4] = COL_WHITE;
	matrix[(i*width)+5] = COL_WHITE;
	matrix[(i*width)+6] = COL_WHITE;
	matrix[(i*width)+7] = COL_WHITE;
	matrix[(i*width)+8] = COL_WHITE;
	matrix[(i*width)+9] = COL_WHITE;
	matrix[(i*width)+10] = 0;
	matrix[(i*width)+11] = 0;
	i++;
	// 6
	matrix[(i*width)+0] = 0;
	matrix[(i*width)+1] = 0;
	matrix[(i*width)+2] = COL_WHITE;
	matrix[(i*width)+3] = ((int)(COL_RED*anim))&COL_RED;
	matrix[(i*width)+4] = ((int)(COL_RED*anim))&COL_RED;
	matrix[(i*width)+5] = COL_WHITE;
	matrix[(i*width)+6] = COL_WHITE;
	matrix[(i*width)+7] = ((int)(COL_RED*anim))&COL_RED;
	matrix[(i*width)+8] = ((int)(COL_RED*anim))&COL_RED;
	matrix[(i*width)+9] = COL_WHITE;
	matrix[(i*width)+10] = 0;
	matrix[(i*width)+11] = 0;
	i++;
	// 7
	matrix[(i*width)+0] = 0;
	matrix[(i*width)+1] = 0;
	matrix[(i*width)+2] = COL_WHITE;
	matrix[(i*width)+3] = ((int)(COL_RED*anim))&COL_RED;
	matrix[(i*width)+4] = ((int)(COL_RED*anim))&COL_RED;
	matrix[(i*width)+5] = COL_WHITE;
	matrix[(i*width)+6] = COL_WHITE;
	matrix[(i*width)+7] = ((int)(COL_RED*anim))&COL_RED;
	matrix[(i*width)+8] = ((int)(COL_RED*anim))&COL_RED;
	matrix[(i*width)+9] = COL_WHITE;
	matrix[(i*width)+10] = 0;
	matrix[(i*width)+11] = 0;
	i++;
	// 8
	matrix[(i*width)+0] = 0;
	matrix[(i*width)+1] = 0;
	matrix[(i*width)+2] = COL_WHITE;
	matrix[(i*width)+3] = COL_WHITE;
	matrix[(i*width)+4] = COL_WHITE;
	matrix[(i*width)+5] = 0;
	matrix[(i*width)+6] = 0;
	matrix[(i*width)+7] = COL_WHITE;
	matrix[(i*width)+8] = COL_WHITE;
	matrix[(i*width)+9] = COL_WHITE;
	matrix[(i*width)+10] = 0;
	matrix[(i*width)+11] = 0;
	i++;
	// 9
	matrix[(i*width)+0] = 0;
	matrix[(i*width)+1] = 0;
	matrix[(i*width)+2] = 0;
	matrix[(i*width)+3] = 0;
	matrix[(i*width)+4] = COL_WHITE;
	matrix[(i*width)+5] = COL_WHITE;
	matrix[(i*width)+6] = COL_WHITE;
	matrix[(i*width)+7] = COL_WHITE;
	matrix[(i*width)+8] = 0;
	matrix[(i*width)+9] = 0;
	matrix[(i*width)+10] = 0;
	matrix[(i*width)+11] = 0;
	i++;
	// 10 
	matrix[(i*width)+0] = 0;
	matrix[(i*width)+1] = 0;
	matrix[(i*width)+2] = 0;
	matrix[(i*width)+3] = 0;
	matrix[(i*width)+4] = COL_WHITE;
	matrix[(i*width)+5] = COL_WHITE;
	matrix[(i*width)+6] = COL_WHITE;
	matrix[(i*width)+7] = COL_WHITE;
	matrix[(i*width)+8] = 0;
	matrix[(i*width)+9] = 0;
	matrix[(i*width)+10] = 0;
	matrix[(i*width)+11] = 0;
	i++;
	
	j = 0;
	while (j < width) {
		matrix[(i*width)+j] = 0;
		j++;
	}
	i++;
	return 0;
}

int render_anim_fire(float anim) {
	
	return 0;
}

enum tetris_pieces {
	TETRIS_SQR,
	TETRIS_LINE,
	TETRIS_T,
	TETRIS_L,
	TETRIS_S,
	TETRIS_IL,
	TETRIS_IS
};

struct tetris_piece {
	int w,h,rot;
	ws2811_led_t col;
	enum tetris_pieces type;
};

/*
struct tetris_gamestate {
	int activepiece;
	int loc;
	struct tetris_piece piece;
	ws2811_led_t gamefield[width*height];
};*/

int tetris_game_end(ws2811_led_t* gamefield) {
	int i;
	if (gamefield == NULL) {
		return -1;
	}
	for (i = 0; i < width; i++) {
		if (gamefield[i] != 0) {
			return 1;
		}
	}
	return 0;
}

int tetris_fit_piece(ws2811_led_t* gamefield, int loc, struct tetris_piece piece, int draw) {
	if (((loc%width)+(piece.w-1) >= width)||((loc/height)+(piece.h-1) >= height)||(loc < 0)) {
		return 0;
	}
	ws2811_led_t* block1 = NULL;
	ws2811_led_t* block2 = NULL;
	ws2811_led_t* block3 = NULL;
	ws2811_led_t* block4 = NULL;
	
	switch (piece.type) {
		case TETRIS_SQR:			
			block1 = &(gamefield[loc]);
			block2 = &(gamefield[loc+1]);
			block3 = &(gamefield[loc+width]);
			block4 = &(gamefield[loc+width+1]);
			if (*block4 != gamefield[loc+width+1]) {
				printf("ERROR IN ASSIGNING POINTERS\n");
				return -1;
			}
			
			break;
		case TETRIS_LINE:
			if ((piece.rot == 0)||(piece.rot == 2)) {
				block1 = &(gamefield[loc]);
				block2 = &(gamefield[loc+1]);
				block3 = &(gamefield[loc+2]);
				block4 = &(gamefield[loc+3]);
				if (*block4 != gamefield[loc+3]) {
					printf("ERROR IN ASSIGNING POINTERS\n");
					return -1;
				}
			}
			else { 
				block1 = &(gamefield[loc]);
				block2 = &(gamefield[loc+width]);
				block3 = &(gamefield[loc+width*2]);
				block4 = &(gamefield[loc+width*3]);
				if (*block4 != gamefield[loc+width*3]) {
					printf("ERROR IN ASSIGNING POINTERS\n");
					return -1;
				}
			}
			break;
		case TETRIS_T:
			if (piece.rot == 0) {
				block1 = &(gamefield[loc]);
				block2 = &(gamefield[loc+1]);
				block3 = &(gamefield[loc+2]);
				block4 = &(gamefield[loc+width+1]);
				if (*block4 != gamefield[loc+width+1]) {
					printf("ERROR IN ASSIGNING POINTERS\n");
					return -1;
				}
			}
			else if (piece.rot == 1) {
				block1 = &(gamefield[loc+1]);
				block2 = &(gamefield[loc+width]);
				block3 = &(gamefield[loc+width+1]);
				block4 = &(gamefield[loc+width*2+1]);
				if (*block4 != gamefield[loc+width*2+1]) {
					printf("ERROR IN ASSIGNING POINTERS\n");
					return -1;
				}
			}
			else if (piece.rot == 2) {
				block1 = &(gamefield[loc+1]);
				block2 = &(gamefield[loc+width]);
				block3 = &(gamefield[loc+width+1]);
				block4 = &(gamefield[loc+width+2]);
				if (*block4 != gamefield[loc+width+2]) {
					printf("ERROR IN ASSIGNING POINTERS\n");
					return -1;
				}
			}
			else {
				block1 = &(gamefield[loc]);
				block2 = &(gamefield[loc+width]);
				block3 = &(gamefield[loc+width+1]);
				block4 = &(gamefield[loc+width*2]);
				if (*block4 != gamefield[loc+width*2]) {
					printf("ERROR IN ASSIGNING POINTERS\n");
					return -1;
				}
			}
			break;
		case TETRIS_L:
			if (piece.rot == 0) {
				block1 = &(gamefield[loc]);
				block2 = &(gamefield[loc+width]);
				block3 = &(gamefield[loc+width*2]);
				block4 = &(gamefield[loc+width*2+1]);
				if (*block4 != gamefield[loc+width*2+1]) {
					printf("ERROR IN ASSIGNING POINTERS\n");
					return -1;
				}
			}
			else if (piece.rot == 1) {
				block1 = &(gamefield[loc]);
				block2 = &(gamefield[loc+1]);
				block3 = &(gamefield[loc+2]);
				block4 = &(gamefield[loc+width]);
				if (*block4 != gamefield[loc+width]) {
					printf("ERROR IN ASSIGNING POINTERS\n");
					return -1;
				}
			}
			else if (piece.rot == 2) {
				block1 = &(gamefield[loc]);
				block2 = &(gamefield[loc+1]);
				block3 = &(gamefield[loc+width+1]);
				block4 = &(gamefield[loc+width*2+1]);
				if (*block4 != gamefield[loc+width*2+1]) {
					printf("ERROR IN ASSIGNING POINTERS\n");
					return -1;
				}
			}
			else {
				block1 = &(gamefield[loc+2]);
				block2 = &(gamefield[loc+width]);
				block3 = &(gamefield[loc+width+1]);
				block4 = &(gamefield[loc+width+2]);
				if (*block4 != gamefield[loc+width+2]) {
					printf("ERROR IN ASSIGNING POINTERS\n");
					return -1;
				}
			}
			break;
		case TETRIS_IL:
			if (piece.rot == 0) {
				block1 = &(gamefield[loc+1]);
				block2 = &(gamefield[loc+width+1]);
				block3 = &(gamefield[loc+width*2]);
				block4 = &(gamefield[loc+width*2+1]);
				if (*block4 != gamefield[loc+width*2+1]) {
					printf("ERROR IN ASSIGNING POINTERS\n");
					return -1;
				}
			}
			else if (piece.rot == 1) {
				block1 = &(gamefield[loc]);
				block2 = &(gamefield[loc+width]);
				block3 = &(gamefield[loc+width+1]);
				block4 = &(gamefield[loc+width+2]);
				if (*block4 != gamefield[loc+width+2]) {
					printf("ERROR IN ASSIGNING POINTERS\n");
					return -1;
				}
			}
			else if (piece.rot == 2) {
				block1 = &(gamefield[loc]);
				block2 = &(gamefield[loc+1]);
				block3 = &(gamefield[loc+width]);
				block4 = &(gamefield[loc+width*2]);
				if (*block4 != gamefield[loc+width*2]) {
					printf("ERROR IN ASSIGNING POINTERS\n");
					return -1;
				}
			}
			else {
				block1 = &(gamefield[loc]);
				block2 = &(gamefield[loc+1]);
				block3 = &(gamefield[loc+2]);
				block4 = &(gamefield[loc+width+2]);
				if (*block4 != gamefield[loc+width+2]) {
					printf("ERROR IN ASSIGNING POINTERS\n");
					return -1;
				}
			}
			break;
		case TETRIS_S:
			if ((piece.rot == 0)&&(piece.rot == 2)) {
				block1 = &(gamefield[loc+1]);
				block2 = &(gamefield[loc+2]);
				block3 = &(gamefield[loc+width]);
				block4 = &(gamefield[loc+width+1]);
				if (*block4 != gamefield[loc+width+1]) {
					printf("ERROR IN ASSIGNING POINTERS\n");
					return -1;
				}
			}
			else {
				block1 = &(gamefield[loc]);
				block2 = &(gamefield[loc+width]);
				block3 = &(gamefield[loc+width+1]);
				block4 = &(gamefield[loc+width*2+1]);
				if (*block4 != gamefield[loc+width*2+1]) {
					printf("ERROR IN ASSIGNING POINTERS\n");
					return -1;
				}
			}
			break;
		case TETRIS_IS:
			if ((piece.rot == 0)||(piece.rot == 2)) {
				block1 = &(gamefield[loc]);
				block2 = &(gamefield[loc+1]);
				block3 = &(gamefield[loc+width+1]);
				block4 = &(gamefield[loc+width+2]);
				if (*block4 != gamefield[loc+width+2]) {
					printf("ERROR IN ASSIGNING POINTERS");
					return -1;
				}
			}
			else {
				block1 = &(gamefield[loc+1]);
				block2 = &(gamefield[loc+width]);
				block3 = &(gamefield[loc+width+1]);
				block4 = &(gamefield[loc+width*2]);
				if (*block4 != gamefield[loc+width*2]) {
					printf("ERROR IN ASSIGNING POINTERS");
					return -1;
				}
			}
			break;
		default:
			return -1;
	}//for different tetris blocks compare different fields
	if (!((*block1 == 0)&&(*block2 == 0)&&(*block3 == 0)&&(*block4 == 0))) {
		return 0;
	}//check if all fields aren't occupied
	if (draw) {
		*block1 = piece.col;
		*block2 = piece.col;
		*block3 = piece.col;
		*block4 = piece.col;
	}
	return 1;
}

struct tetris_piece get_random_tetris_piece() {
	enum tetris_pieces t = TETRIS_SQR;
	int w = 0;
	int h = 0;
	switch (rand()%8) {
		case 0:
			t = TETRIS_SQR;
			w = 2;
			h = 2;
			break;
		case 1:
			t = TETRIS_LINE;
			w = 4;
			h = 1;
			break;
		case 2:
			t = TETRIS_T;
			w = 3;
			h = 2;
			break;
		case 3:
			t = TETRIS_L;
			w = 2;
			h = 3;
			break;
		case 4:
			t = TETRIS_IL;
			w = 2;
			h = 3;
			break;
		case 5:
			t = TETRIS_S;
			w = 3;
			h = 2;
			break;
		case 6:
			t = TETRIS_IS;
			w = 3;
			h = 2;
			break;
	}
	struct tetris_piece piece = {
		.col = ((rand()%256)<<16) + ((rand()%256)<<8) + (rand()%256),
		.type = t,
		.w = w,
		.h = h,
		.rot = 0
	};
	return piece;
}

void draw_tetris_piece (struct tetris_piece piece, int loc) {
	ws2811_led_t* block1 = NULL;
	ws2811_led_t* block2 = NULL;
	ws2811_led_t* block3 = NULL;
	ws2811_led_t* block4 = NULL;
	switch (piece.type) {
		case TETRIS_SQR:			
			block1 = &(matrix[loc]);
			block2 = &(matrix[loc+1]);
			block3 = &(matrix[loc+width]);
			block4 = &(matrix[loc+width+1]);
			if (*block4 != matrix[loc+width+1]) {
				//printf("ERROR IN ASSIGNING POINTERS\n");
				return;
			}
			
			break;
		case TETRIS_LINE:
			if ((piece.rot == 0)||(piece.rot == 2)) {
				block1 = &(matrix[loc]);
				block2 = &(matrix[loc+1]);
				block3 = &(matrix[loc+2]);
				block4 = &(matrix[loc+3]);
				if (*block4 != matrix[loc+3]) {
					//printf("ERROR IN ASSIGNING POINTERS\n");
					return;
				}
			}
			else { 
				block1 = &(matrix[loc]);
				block2 = &(matrix[loc+width]);
				block3 = &(matrix[loc+width*2]);
				block4 = &(matrix[loc+width*3]);
				if (*block4 != matrix[loc+width*3]) {
					//printf("ERROR IN ASSIGNING POINTERS\n");
					return;
				}
			}
			break;
		case TETRIS_T:
			if (piece.rot == 0) {
				block1 = &(matrix[loc]);
				block2 = &(matrix[loc+1]);
				block3 = &(matrix[loc+2]);
				block4 = &(matrix[loc+width+1]);
				if (*block4 != matrix[loc+3]) {
					//printf("ERROR IN ASSIGNING POINTERS\n");
					return;
				}
			}
			else if (piece.rot == 1) {
				block1 = &(matrix[loc+1]);
				block2 = &(matrix[loc+width]);
				block3 = &(matrix[loc+width+1]);
				block4 = &(matrix[loc+width*2+1]);
				if (*block4 != matrix[loc+width*2+1]) {
					//printf("ERROR IN ASSIGNING POINTERS\n");
					return;
				}
			}
			else if (piece.rot == 2) {
				block1 = &(matrix[loc+1]);
				block2 = &(matrix[loc+width]);
				block3 = &(matrix[loc+width+1]);
				block4 = &(matrix[loc+width+2]);
				if (*block4 != matrix[loc+width+2]) {
					//printf("ERROR IN ASSIGNING POINTERS\n");
					return;
				}
			}
			else {
				block1 = &(matrix[loc]);
				block2 = &(matrix[loc+width]);
				block3 = &(matrix[loc+width+1]);
				block4 = &(matrix[loc+width*2]);
				if (*block4 != matrix[loc+width*2]) {
					//printf("ERROR IN ASSIGNING POINTERS\n");
					return;
				}
			}
			break;
		case TETRIS_L:
			if (piece.rot == 0) {
				block1 = &(matrix[loc]);
				block2 = &(matrix[loc+width]);
				block3 = &(matrix[loc+width*2]);
				block4 = &(matrix[loc+width*2+1]);
				if (*block4 != matrix[loc+width*2+1]) {
					//printf("ERROR IN ASSIGNING POINTERS\n");
					return;
				}
			}
			else if (piece.rot == 1) {
				block1 = &(matrix[loc]);
				block2 = &(matrix[loc+1]);
				block3 = &(matrix[loc+2]);
				block4 = &(matrix[loc+width]);
				if (*block4 != matrix[loc+width]) {
					//printf("ERROR IN ASSIGNING POINTERS\n");
					return;
				}
			}
			else if (piece.rot == 2) {
				block1 = &(matrix[loc]);
				block2 = &(matrix[loc+1]);
				block3 = &(matrix[loc+width+1]);
				block4 = &(matrix[loc+width*2+1]);
				if (*block4 != matrix[loc+width*2+1]) {
					//printf("ERROR IN ASSIGNING POINTERS\n");
					return;
				}
			}
			else {
				block1 = &(matrix[loc+2]);
				block2 = &(matrix[loc+width]);
				block3 = &(matrix[loc+width+1]);
				block4 = &(matrix[loc+width+2]);
				if (*block4 != matrix[loc+width+2]) {
					//printf("ERROR IN ASSIGNING POINTERS\n");
					return;
				}
			}
			break;
		case TETRIS_IL:
			if (piece.rot == 0) {
				block1 = &(matrix[loc+1]);
				block2 = &(matrix[loc+width+1]);
				block3 = &(matrix[loc+width*2]);
				block4 = &(matrix[loc+width*2+1]);
				if (*block4 != matrix[loc+width*2+1]) {
					//printf("ERROR IN ASSIGNING POINTERS\n");
					return;
				}
			}
			else if (piece.rot == 1) {
				block1 = &(matrix[loc]);
				block2 = &(matrix[loc+width]);
				block3 = &(matrix[loc+width+1]);
				block4 = &(matrix[loc+width+2]);
				if (*block4 != matrix[loc+width+2]) {
					//printf("ERROR IN ASSIGNING POINTERS\n");
					return;
				}
			}
			else if (piece.rot == 2) {
				block1 = &(matrix[loc]);
				block2 = &(matrix[loc+1]);
				block3 = &(matrix[loc+width]);
				block4 = &(matrix[loc+width*2]);
				if (*block4 != matrix[loc+width*2]) {
					//printf("ERROR IN ASSIGNING POINTERS\n");
					return;
				}
			}
			else {
				block1 = &(matrix[loc]);
				block2 = &(matrix[loc+1]);
				block3 = &(matrix[loc+2]);
				block4 = &(matrix[loc+width+2]);
				if (*block4 != matrix[loc+width+2]) {
					//printf("ERROR IN ASSIGNING POINTERS\n");
					return;
				}
			}
			break;
		case TETRIS_S:
			if ((piece.rot == 0)&&(piece.rot == 2)) {
				block1 = &(matrix[loc+1]);
				block2 = &(matrix[loc+2]);
				block3 = &(matrix[loc+width]);
				block4 = &(matrix[loc+width+1]);
				if (*block4 != matrix[loc+width+1]) {
					//printf("ERROR IN ASSIGNING POINTERS\n");
					return;
				}
			}
			else {
				block1 = &(matrix[loc]);
				block2 = &(matrix[loc+width]);
				block3 = &(matrix[loc+width+1]);
				block4 = &(matrix[loc+width*2+1]);
				if (*block4 != matrix[loc+width*2+1]) {
					//printf("ERROR IN ASSIGNING POINTERS\n");
					return;
				}
			}
			break;
		case TETRIS_IS:
			if ((piece.rot == 0)||(piece.rot == 2)) {
				block1 = &(matrix[loc]);
				block2 = &(matrix[loc+1]);
				block3 = &(matrix[loc+width+1]);
				block4 = &(matrix[loc+width+2]);
				if (*block4 != matrix[loc+width+2]) {
					//printf("ERROR IN ASSIGNING POINTERS");
					return;
				}
			}
			else {
				block1 = &(matrix[loc+1]);
				block2 = &(matrix[loc+width]);
				block3 = &(matrix[loc+width+1]);
				block4 = &(matrix[loc+width*2]);
				if (*block4 != matrix[loc+width*2]) {
					//printf("ERROR IN ASSIGNING POINTERS");
					return;
				}
			}
			break;
	}
	*block1 = piece.col;
	*block2 = piece.col;
	*block3 = piece.col;
	*block4 = piece.col;
}

void rotate_tetris_piece(struct tetris_piece piece) {
	piece.rot += 1;
	int tmp = piece.w;
	piece.w = piece.h;
	piece.h = tmp;
	piece.rot = piece.rot%4;
}

void draw_tetris_gamefield(ws2811_led_t* gamefield) {
	int i;
	for (i = 0; i < width*height; i++) {
		matrix[i] = gamefield[i];
	}
}

void clear_tetris_gamefield(ws2811_led_t* gamefield) {
	int i;
	for (i = 0; i < width*height; i++) {
		gamefield[i] = 0;
	}
}
 
int render_anim_tetris(int speed) {
	ws2811_led_t gamefield[width*height];
	clear_tetris_gamefield(gamefield);
	//int activepiece = 0;
	while (running&&(!tetris_game_end(gamefield))) {
		usleep(1000000/speed);
		matrix_clear();
		struct tetris_piece piece = get_random_tetris_piece();
		int loc = rand()%(width-(piece.w-1));
		while (tetris_fit_piece(gamefield,loc,piece,0)&&running) {
			usleep(1000000/speed);
			draw_tetris_gamefield(gamefield);
			draw_tetris_piece(piece,loc);
			matrix_render();
			loc += width;
			//usleep(1000000/speed);
			
			//printf("falling piece...\n");
			
		}
		loc -= width;
		tetris_fit_piece(gamefield,loc,piece,1);
		draw_tetris_gamefield(gamefield);
		matrix_render();
		//printf("generate new piece...\n");
		
	}
	
	
	return 0;
}

void matrix_raise(void)
{
    int x, y;

    for (y = 0; y < (height - 1); y++)
    {
        for (x = 0; x < width; x++)
        {
            matrix[y * width + x] = matrix[(y + 1)*width + x];
        }
    }
}

void matrix_bottom(void)
{
    int i;

    for (i = 0; i < ARRAY_SIZE(dotspos); i++)
    {
        dotspos[i]++;
        if (dotspos[i] > (width - 1))
        {
            dotspos[i] = 0;
        }

			if (ledstring.channel[0].strip_type == SK6812_STRIP_RGBW) {
				matrix[dotspos[i] + (height - 1) * width] = dotcolors_rgbw[i];
			} else {
				matrix[dotspos[i] + (height - 1) * width] = dotcolors[i];
			}
    }
}

static void ctrl_c_handler(int signum)
{
    running = 0;
}

static void setup_handlers(void)
{
    struct sigaction sa =
    {
        .sa_handler = ctrl_c_handler,
    };

    sigaction(SIGINT, &sa, NULL);
    sigaction(SIGTERM, &sa, NULL);
}


void parseargs(int argc, char **argv, ws2811_t *ws2811)
{
	int index;
	int c;

	static struct option longopts[] =
	{
		{"help", no_argument, 0, 'h'},
		{"dma", required_argument, 0, 'd'},
		{"gpio", required_argument, 0, 'g'},
		{"invert", no_argument, 0, 'i'},
		{"clear", no_argument, 0, 'c'},
		{"strip", required_argument, 0, 's'},
		{"height", required_argument, 0, 'y'},
		{"width", required_argument, 0, 'x'},
		{"version", no_argument, 0, 'v'},
		{0, 0, 0, 0}
	};

	while (1)
	{

		index = 0;
		c = getopt_long(argc, argv, "cd:g:his:vx:y:", longopts, &index);

		if (c == -1)
			break;

		switch (c)
		{
		case 0:
			/* handle flag options (array's 3rd field non-0) */
			break;

		case 'h':
			fprintf(stderr, "%s version %s\n", argv[0], VERSION);
			fprintf(stderr, "Usage: %s \n"
				"-h (--help)    - this information\n"
				"-s (--strip)   - strip type - rgb, grb, gbr, rgbw\n"
				"-x (--width)   - matrix width (default 8)\n"
				"-y (--height)  - matrix height (default 8)\n"
				"-d (--dma)     - dma channel to use (default 5)\n"
				"-g (--gpio)    - GPIO to use must be one of 12,18,40,52\n"
				"                 If omitted, default is 18\n"
				"-i (--invert)  - invert pin output (pulse LOW)\n"
				"-c (--clear)   - clear matrix on exit.\n"
				"-v (--version) - version information\n"
				, argv[0]);
			exit(-1);

		case 'D':
			break;

		case 'g':
			if (optarg) {
				int gpio = atoi(optarg);
/*
	https://www.raspberrypi.org/forums/viewtopic.php?f=91&t=105044
	PWM0, which can be set to use GPIOs 12, 18, 40, and 52. 
	Only 12 (pin 32) and 18 (pin 12) are available on the B+/2B
	PWM1 which can be set to use GPIOs 13, 19, 41, 45 and 53. 
	Only 13 is available on the B+/2B, on pin 35
*/
				switch (gpio) {
					case 12:
					case 18:
					case 40:
					case 52:
						ws2811->channel[0].gpionum = gpio;
						break;
					default:
						printf ("gpio %d doesnt support PWM0\n",gpio);
						exit (-1);
				}
			}
			break;

		case 'i':
			ws2811->channel[0].invert=1;
			break;

		case 'c':
			clear_on_exit=1;
			break;

		case 'd':
			if (optarg) {
				int dma = atoi(optarg);
				if (dma < 14) {
					ws2811->dmanum = dma;
				} else {
					printf ("invalid dma %d\n", dma);
					exit (-1);
				}
			}
			break;

		case 'y':
			if (optarg) {
				height = atoi(optarg);
				if (height > 0) {
					ws2811->channel[0].count = height * width;
				} else {
					printf ("invalid height %d\n", height);
					exit (-1);
				}
			}
			break;

		case 'x':
			if (optarg) {
				width = atoi(optarg);
				if (width > 0) {
					ws2811->channel[0].count = height * width;
				} else {
					printf ("invalid width %d\n", width);
					exit (-1);
				}
			}
			break;

		case 's':
			if (optarg) {
				if (!strncasecmp("rgb", optarg, 4)) {
					ws2811->channel[0].strip_type = WS2811_STRIP_RGB;
				}
				else if (!strncasecmp("rbg", optarg, 4)) {
					ws2811->channel[0].strip_type = WS2811_STRIP_RBG;
				}
				else if (!strncasecmp("grb", optarg, 4)) {
					ws2811->channel[0].strip_type = WS2811_STRIP_GRB;
				}
				else if (!strncasecmp("gbr", optarg, 4)) {
					ws2811->channel[0].strip_type = WS2811_STRIP_GBR;
				}
				else if (!strncasecmp("brg", optarg, 4)) {
					ws2811->channel[0].strip_type = WS2811_STRIP_BRG;
				}
				else if (!strncasecmp("bgr", optarg, 4)) {
					ws2811->channel[0].strip_type = WS2811_STRIP_BGR;
				}
				else if (!strncasecmp("rgbw", optarg, 4)) {
					ws2811->channel[0].strip_type = SK6812_STRIP_RGBW;
				}
				else if (!strncasecmp("grbw", optarg, 4)) {
					ws2811->channel[0].strip_type = SK6812_STRIP_GRBW;
				}
				else {
					printf ("invalid strip %s\n", optarg);
					exit (-1);
				}
			}
			break;

		case 'v':
			fprintf(stderr, "%s version %s\n", argv[0], VERSION);
			exit(-1);

		case '?':
			/* getopt_long already reported error? */
			exit(-1);

		default:
			exit(-1);
		}
	}
}


int main(int argc, char *argv[])
{
    ws2811_return_t ret;

    parseargs(argc, argv, &ledstring);

    matrix = malloc(sizeof(ws2811_led_t) * width * height);
    //adj_matrix = malloc(sizeof(ws2811_led_t) * width * height);

    setup_handlers();

    if ((ret = ws2811_init(&ledstring)) != WS2811_SUCCESS)
    {
        fprintf(stderr, "ws2811_init failed: %s\n", ws2811_get_return_t_str(ret));
        return ret;
    }
    //int sw = 0;
    //ws2811_led_t color = 1;
    int loop_var = 0;
    while (running)
    {
				render_anim_tetris(4);
        //matrix_raise();
        //matrix_bottom();
        //render_image_skull(sin(((float)loop_var)));
        //int j;
        //ws2811_led_t color = 0;
	/*
        for (j = 0; j < width*height; j++) {
			matrix[j] = color;
			color++;
			if (color > 0xFFFFFFFE) {
				color = 0;
			}
		}
	*/
		//sw = (sw ? 0: 1);
        //matrix_render();
				
				/*
        if ((ret = ws2811_render(&ledstring)) != WS2811_SUCCESS)
        {
            fprintf(stderr, "ws2811_render failed: %s\n", ws2811_get_return_t_str(ret));
            break;
        }
        */

        // 15 frames /sec
        loop_var++;
        usleep(1000000 / 1000);
    }

    if (clear_on_exit) {
	matrix_clear();
	matrix_render();
	ws2811_render(&ledstring);
    }

    ws2811_fini(&ledstring);
    //free(adj_matrix);
    printf ("\n");
    return ret;
}

