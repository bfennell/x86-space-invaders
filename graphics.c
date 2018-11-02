/*
  Copyright (c) 2018 Brendan Fennell <bfennell@skynet.ie>

  Permission is hereby granted, free of charge, to any person obtaining a copy
  of this software and associated documentation files (the "Software"), to deal
  in the Software without restriction, including without limitation the rights
  to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
  copies of the Software, and to permit persons to whom the Software is
  furnished to do so, subject to the following conditions:

  The above copyright notice and this permission notice shall be included in all
  copies or substantial portions of the Software.

  THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
  IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
  FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
  AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
  LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
  OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
  SOFTWARE.
*/

#include <stdint.h>
#include <stdbool.h>

#include "multiboot.h"
#include "x86.h"
#include "i8080.h"
#include "stdio.h"

#include "graphics.h"

#define i8080_VRAM_BUFFER_ADDR 0x2400
#define i8080_VRAM_WIDTH  256
#define i8080_VRAM_HEIGHT 224

#define i8080_FONT_DATA_ADDR 0x1e00
#define i8080_FONT_WIDTH  8
#define i8080_FONT_HEIGHT 8

#define YELLOW 0x00ffff00
#define WHITE  0xffffffff

#define PRINT_BUF_SIZE 256

static void graphics_update (void);

/* Screen frame buffer pointer */
static uint32_t* screen_fb;

/* Screen resolution */
static int screen_width;
static int screen_height;

/* i8080 cpu state structure */
static i8080_state_t* i8080_state_ptr;

typedef struct point {
    int x;
    int y;
} point_t;

typedef struct font_entry {
    int character;
    int offset;
} font_entry_t;

/* current location of the text cursor */
static point_t cursor;

/* The Space Invader font is 8x8 pixels. This table defines the offsets into
   the start of the font data for each charater. Each charater consists of
   8 bytes of data.
*/
#define FONT_TABLE_ENTRY_OFFSET(x) ((x)*8)
static font_entry_t font_table[] = {
    {'a', FONT_TABLE_ENTRY_OFFSET(0)},
    {'b', FONT_TABLE_ENTRY_OFFSET(1)},
    {'c', FONT_TABLE_ENTRY_OFFSET(2)},
    {'d', FONT_TABLE_ENTRY_OFFSET(3)},
    {'e', FONT_TABLE_ENTRY_OFFSET(4)},
    {'f', FONT_TABLE_ENTRY_OFFSET(5)},
    {'g', FONT_TABLE_ENTRY_OFFSET(6)},
    {'h', FONT_TABLE_ENTRY_OFFSET(7)},
    {'i', FONT_TABLE_ENTRY_OFFSET(8)},
    {'j', FONT_TABLE_ENTRY_OFFSET(9)},
    {'k', FONT_TABLE_ENTRY_OFFSET(10)},
    {'l', FONT_TABLE_ENTRY_OFFSET(11)},
    {'m', FONT_TABLE_ENTRY_OFFSET(12)},
    {'n', FONT_TABLE_ENTRY_OFFSET(13)},
    {'o', FONT_TABLE_ENTRY_OFFSET(14)},
    {'p', FONT_TABLE_ENTRY_OFFSET(15)},
    {'q', FONT_TABLE_ENTRY_OFFSET(16)},
    {'r', FONT_TABLE_ENTRY_OFFSET(17)},
    {'s', FONT_TABLE_ENTRY_OFFSET(18)},
    {'t', FONT_TABLE_ENTRY_OFFSET(19)},
    {'u', FONT_TABLE_ENTRY_OFFSET(20)},
    {'v', FONT_TABLE_ENTRY_OFFSET(21)},
    {'w', FONT_TABLE_ENTRY_OFFSET(22)},
    {'x', FONT_TABLE_ENTRY_OFFSET(23)},
    {'y', FONT_TABLE_ENTRY_OFFSET(24)},
    {'z', FONT_TABLE_ENTRY_OFFSET(25)},
    {'0', FONT_TABLE_ENTRY_OFFSET(26)},
    {'1', FONT_TABLE_ENTRY_OFFSET(27)},
    {'2', FONT_TABLE_ENTRY_OFFSET(28)},
    {'3', FONT_TABLE_ENTRY_OFFSET(29)},
    {'4', FONT_TABLE_ENTRY_OFFSET(30)},
    {'5', FONT_TABLE_ENTRY_OFFSET(31)},
    {'6', FONT_TABLE_ENTRY_OFFSET(32)},
    {'7', FONT_TABLE_ENTRY_OFFSET(33)},
    {'8', FONT_TABLE_ENTRY_OFFSET(34)},
    {'9', FONT_TABLE_ENTRY_OFFSET(35)},
    {'<', FONT_TABLE_ENTRY_OFFSET(36)},
    {'>', FONT_TABLE_ENTRY_OFFSET(37)},
    {' ', FONT_TABLE_ENTRY_OFFSET(38)},
    {'=', FONT_TABLE_ENTRY_OFFSET(39)},
    {'*', FONT_TABLE_ENTRY_OFFSET(40)},
    /* ... */
    {'?', FONT_TABLE_ENTRY_OFFSET(56)},
    /* ... */
    {'-', FONT_TABLE_ENTRY_OFFSET(63)},
};

/* map of ascii value to index in font_table */
static int font_map[128]; /* ascii characters 0...127 */

static void graphics_show_info (multiboot_info_t *mbi)
{
    if (mbi->flags & MULTIBOOT_INFO_FRAMEBUFFER_INFO) {
        unsigned fb_addr = (unsigned)mbi->framebuffer_addr;

        printf ("VBE: 0x%08x 0x%08x 0x%08x 0x%08x 0x%08x 0x%08x\n",
                mbi->vbe_control_info,
                mbi->vbe_mode_info,
                mbi->vbe_mode,
                mbi->vbe_interface_seg,
                mbi->vbe_interface_off,
                mbi->vbe_interface_len);

        printf ("FB:  0x%08x 0x%08x 0x%08x 0x%08x 0x%08x 0x%08x\n",
                fb_addr,
                mbi->framebuffer_pitch,
                mbi->framebuffer_width,
                mbi->framebuffer_height,
                mbi->framebuffer_bpp,
                mbi->framebuffer_type);
    }
}

void timer_irq_handler (void)
{
    i8080_state_ptr->irq_set_cnt++;
    /* update the frame buffer on the end of screen interrupt */
    if ((i8080_state_ptr->irq_set_cnt & 1) == 0) {
        graphics_update();
    }
}

static void timer_init (uint32_t frequency)
{
    uint32_t divisor = 1193180 / frequency;
    outport8(0x43, 0x36);
    uint8_t l = (uint8_t)(divisor & 0xff);
    uint8_t h = (uint8_t)((divisor>>8) & 0xff);
    outport8(0x40, l);
    outport8(0x40, h);
}

void graphics_init (multiboot_info_t *mbi, i8080_state_t* state)
{
    i8080_state_ptr = state;

    screen_fb = pointer_cast(uint32_t*,mbi->framebuffer_addr);
    screen_width = mbi->framebuffer_width;
    screen_height = mbi->framebuffer_height;

    graphics_show_info (mbi);

    /* fill font_map */
    int qmark_idx = (sizeof(font_table)/sizeof(font_table[0])) - 1;
    for (unsigned i = 0; i < (sizeof(font_map)/sizeof(font_map[0])); ++i) {
        int character = i;

        /* if A...Z convert to lower case */
        if (character >= 0x41 && character <= 0x5a) {
            character += 0x20;
        }

        font_map[character] = qmark_idx; /* default */
        for (unsigned j = 0; j < (sizeof(font_table)/sizeof(font_table[0])); ++j) {
            if (font_table[j].character == character) {
                font_map[i] = j;
                break;
            }
        }
    }

    timer_init (120);  /* ~8.33mS */
}

static inline bool get_pixel (uint8_t* pixels, const int row, const int col, const int width)
{
    int idx = (row*width/8)+(col/8);
    uint8_t buf = pixels[idx];
    return ((buf >> ((row*width+col) % 8)) & 1);
}

static inline void set_pixel (uint32_t* pixels, const int row, const int col, const uint32_t colour)
{
    pixels[row*screen_width + col] = colour;
}

/* Copy the pixel data from the i8080 vram buffer to the screen frame buffer.
   While copying the vram data is rotated -90 degress due to the fact that the
   origional Space Invaders hardware had the display on its side. The x_delta
   ensures that the game is centered on the screen. Each row of the space
   invaders pixel data is copied to each column of the screen frame buffer,
   while converting the single bit per pixel data into 32 bits per pixel.
*/
static void graphics_draw_block (uint8_t* pixels, point_t pos, int width, int height, uint32_t colour, bool center)
{
    const int scale = 1;
    int col_shift = center ? ((screen_width/2) - (width*scale/2)) : 0;

    for (int row = 0; row < height; ++row) {
        for (int col = 0; col < width; ++col) {
            /* Rotation of -90 degress requires copying pixel buffer rows to frame buffer columns.
               Pixel data is read in rows from the Space Invaders VRAM buffer and copied into colums
               in the screen frame buffer.
            */
            const int srow = pos.y + width-col;
            const int scol = pos.x + col_shift + row;
            uint32_t c = get_pixel (pixels, row, col, width) ? colour : 0;

            set_pixel (screen_fb, srow, scol, c);
        }
    }
}

static void graphics_update (void)
{
    uint8_t* pixels = &i8080_state_ptr->mem[i8080_VRAM_BUFFER_ADDR];
    point_t pos = {.x = 0, .y = 0};
    int width = i8080_VRAM_WIDTH;
    int height = i8080_VRAM_HEIGHT;

    graphics_draw_block (pixels, pos, width, height, WHITE, true);
}

/* display a character on the screen */
int graphics_putchar (int c)
{
    int width = i8080_FONT_WIDTH;
    int height = i8080_FONT_HEIGHT;

    if (c != '\n') {
        const uint8_t ascii_mask = 0x7f; /* 0...127 */
        uint8_t* pixels = &i8080_state_ptr->mem[i8080_FONT_DATA_ADDR];

        pixels += font_table[font_map[c & ascii_mask]].offset;

        graphics_draw_block (pixels, cursor, width, height, YELLOW, false);
    }

    if (cursor.x >= screen_width || c == '\n') {
        cursor.y += screen_width * (width+2/* 2 pixels space between rows */);
        cursor.x = 0;
    } else {
        cursor.x += width;
    }

    return c;
}

int graphics_printf (const char *format, ...)
{
    char buf[PRINT_BUF_SIZE];
    va_list args;
    int count;

    va_start(args,format);
    count = vsnprintf (buf, PRINT_BUF_SIZE, format, args);
    va_end(args);

    int i = 0;
    while (buf[i]) {
        if (buf[i] == '\r') {
            cursor.x = 0;
            ++i;
            continue;
        }
        graphics_putchar(buf[i++]);
    }

    return count;
}
