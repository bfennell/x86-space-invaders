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

#include "multiboot.h"
#include "x86.h"
#include "stdio.h"
#include "i8080.h"
#include "invaders_io.h"
#include "graphics.h"
#include "keyboard.h"
#include "bdos.h"

/* i8080 hardware  */
#define i8080_RAM_SIZE (64*1024) /* 64kiB */

/* first word of the rom images used for identification */
#define i8080_CPUDIAG_MAGIC  0x4d01abc3
#define i8080_INVADERS_MAGIC 0xc3000000
#define i8080_UNKNOWN_MAGIC  0xffffffff

static uint32_t get_rom_image (multiboot_info_t *mbi, uint8_t** image, int* len);
static void exec_cpudiag (i8080_state_t* state, uint8_t* image, int image_len);
static void exec_invaders (i8080_state_t* state, uint8_t* image, int image_len);

/* i8080 state structure and memory */
static i8080_state_t i8080_state;
static uint8_t i8080_ram[i8080_RAM_SIZE];

/* defined in start.S */
extern multiboot_info_t* multiboot_ptr;

int main (void)
{
    show_cpu_info();

    i8080_init (&i8080_state, i8080_ram, i8080_RAM_SIZE);

    uint8_t* image;
    int image_len;
    uint32_t magic = get_rom_image (multiboot_ptr, &image, &image_len);

    switch (magic) {
    case i8080_CPUDIAG_MAGIC:
        exec_cpudiag (&i8080_state, image, image_len);
        break;
    case i8080_INVADERS_MAGIC:
        exec_invaders (&i8080_state, image, image_len);
        break;
    default:
        printf ("[error]: unknown image specified...\n");
        break;
    }

    return 0;
}

static void exec_cpudiag (i8080_state_t* state, uint8_t* image, int image_len)
{
    int cpudiag_load_address = 0x100;

    printf ("Loading cpudiag...\n");
    i8080_load_memory (state, cpudiag_load_address, image, image_len);
    i8080_set_pc (state, cpudiag_load_address);
    i8080_set_instr_handler (state, bdos_entry);

    printf ("Executing 8080 image...\n");
    while (!i8080_exec (state));
    printf ("\n*** 8080 CPU HALTED ***\n");
}

static void exec_invaders (i8080_state_t* state, uint8_t* image, int image_len)
{
    int invaders_load_address = 0x000;

    graphics_init (multiboot_ptr, state);
    keyboard_init (io_keyevent_fn);
    io_init(state);
    i8080_set_io_handler (state, io_handler);
    printf ("Loading invaders...\n");
    i8080_load_memory (state, invaders_load_address, image, image_len);

    irq_enable();

    printf ("Executing 8080 image...\n");
    while (!i8080_exec (state)) {
        if (state->irq_set_cnt != state->irq_clr_cnt) {
            if ((state->irq_set_cnt & 1) == 0) {
                i8080_interrupt (state, 2); /* end of screen interrupt */
            } else {
                i8080_interrupt (state, 1); /* mid screen interrupt */
            }
            state->irq_clr_cnt++;
        }
    }
    irq_disable();
    printf ("*** 8080 CPU HALTED ***\n");
    graphics_printf ("*** 8080 CPU HALTED ***\n");
}

static uint32_t get_rom_image (multiboot_info_t *mbi, uint8_t** image, int* len)
{
    uint32_t magic = i8080_UNKNOWN_MAGIC;
    if (mbi->flags & MULTIBOOT_INFO_MODS) {
        multiboot_module_t *mod = pointer_cast(multiboot_module_t*,mbi->mods_addr);
        *image = pointer_cast(uint8_t*,mod->mod_start);
        *len = (int)(mod->mod_end - mod->mod_start);
        magic = *pointer_cast(uint32_t*,mod->mod_start);
    }
    return magic;
}
