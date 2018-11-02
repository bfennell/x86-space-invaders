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
#include <string.h>

#include "i8080.h"
#include "stdio.h"

#include "invaders_io.h"

#define PORT2_SHIFT_AMT 0x03

#define PORT3_UFO        0x01 /* SX0 0.raw */
#define PORT3_SHOT       0x02 /* SX1 1.raw */
#define PORT3_FLASH      0x04 /* SX2 2.raw */
#define PORT3_INVADER    0x08 /* SX3 3.raw */
#define PORT3_EXTEND     0x10 /* SX4 */
#define PORT3_AMP_ENABLE 0x20 /* SX5 */

#define PORT5_FLEET_1 0x01 /* SX6  4.raw */
#define PORT5_FLEET_2 0x02 /* SX7  5.raw */
#define PORT5_FLEET_3 0x04 /* SX8  6.raw */
#define PORT5_FLEET_4 0x08 /* SX9  7.raw */
#define PORT5_UFO_HIT 0x10 /* SX10 8.raw */

struct input_ports
{
    /* port 0 */
    unsigned dip4:1;    /* power up self-test */
    unsigned bit01:1;   /* always 1 */
    unsigned bit02:1;   /* always 1 */
    unsigned bit03:1;   /* always 1 */
    unsigned fire:1;
    unsigned left:1;
    unsigned right:1;
    unsigned bit07:1;   /* MYSTERY? */
    /* port 1 */
    unsigned credit:1;
    unsigned p2:1;      /* Player 2 start */
    unsigned p1:1;      /* Player 1 start */
    unsigned bit13:1;   /* always 1 */
    unsigned p1shot:1;
    unsigned p1left:1;
    unsigned p1right:1;
    unsigned bit17:1;   /* MYSTERY? */
    /* port 2 */
    unsigned dip3:1;
    unsigned dip5:1;
    unsigned tilt:1;
    unsigned dip6:1;
    unsigned p2shot:1;
    unsigned p2left:1;
    unsigned p2right:1;
    unsigned dip7:1;    /* Coin info in demo screen */
} inputs;

/* i8080 cpu state structure */
static i8080_state_t* i8080_state_ptr;

void io_init (i8080_state_t* state)
{
    i8080_state_ptr = state;

    memset (&inputs, 0, sizeof (struct input_ports));

    inputs.bit01 = 1; /* always 1 */
    inputs.bit02 = 1; /* always 1 */
    inputs.bit03 = 1; /* always 1 */
    inputs.bit13 = 1; /* always 1 */
    /* Dip3Dip5: 0b00 => 3 ships
                 0b01 => 4 ships
                 0b10 => 5 ships
                 0b11 => 6 ships
    */
    inputs.dip3 = 1;
    inputs.dip5 = 1;
    /* Dip6: 0 => extra ship at 1500, 1 => extra ship at 1000 */
    inputs.dip6 = 1;
}

uint8_t io_handler (const uint8_t port, const uint8_t byte, const int direction)
{
    static uint16_t shift_reg = 0;
    static int shift_off = 0;
    uint8_t ret = 0;

    if (direction == DEVICE_IN) {
        /*
          Space Invaders 8080's read ports:
             Read
                00 INPUTS (Mapped in hardware but never used by the code)
                01 INPUTS
                02 INPUTS
                03 bit shift register read
        */
        switch (port) {
            case 0: {
                /* unused port */
                break;
            }
            case 1: {
                ret = ((inputs.credit  << 0) | (inputs.p2     << 1) | (inputs.p1     << 2) |
                       (inputs.bit13   << 3) | (inputs.p1shot << 4) | (inputs.p1left << 5) |
                       (inputs.p1right << 6) | (inputs.bit17  << 7));
                break;
            }
            case 2: {
                ret = ((inputs.dip3    << 0) | (inputs.dip5   << 1) | (inputs.tilt   << 2) |
                       (inputs.dip6    << 3) | (inputs.p2shot << 4) | (inputs.p2left << 5) |
                       (inputs.p2right << 6) | (inputs.dip7   << 7));
                break;
            }
            case 3: {
                ret = ((shift_reg >> (8 - shift_off)) & 0xff);
                break;
            }
            default: {
                printf ("[error] unknown input port: %02x\n", port);
                i8080_state_ptr->halt_req = 1;
            }
        }
    } else { // DEVICE_OUT
        /*
          Space Invaders 8080's write ports:
             Write
                02 shift amount (3 bits)
                03 sound bits
                04 shift data
                05 sound bits
                06 watch-dog
        */
        switch (port) {
            case 2: {
                shift_off = (byte & 0x7);
                break;
            }
            case 3: {
                /*
                  bit 0=UFO (repeats) SX0 0.raw
                  bit 1=Shot SX1 1.raw
                  bit 2=Flash (player die) SX2 2.raw
                  bit 3=Invader die SX3 3.raw
                  bit 4=Extended play SX4
                  bit 5= AMP enable SX5
                  bit 6= NC (not wired)
                  bit 7= NC (not wired)
                 */
                /* Not implemented. */
                break;
            }
            case 4: { // shift x -> y and byte -> x
                shift_reg >>= 8;
                shift_reg |= (byte << 8);
                break;
            }
            case 5: {
                /*
                  bit 0=Fleet movement 1 SX6 4.raw
                  bit 1=Fleet movement 2 SX7 5.raw
                  bit 2=Fleet movement 3 SX8 6.raw
                  bit 3=Fleet movement 4 SX9 7.raw
                  bit 4=UFO Hit SX10 8.raw
                  bit 5= NC (Cocktail mode control ... to flip screen)
                  bit 6= NC (not wired)
                  bit 7= NC (not wired)
                 */
                /* Not implemented. */
                break;
            }
            case 6: {
                /* Watchdog, read/write to reset */
                /* Not implemented. */
                break;
            }
            default: {
                printf ("[error] unknown output port: %02x\n", port);
                i8080_state_ptr->halt_req = 1;
            }
        }
    }

    return ret;
}

void io_keyevent_fn (const key_t key, const keyevent_t event)
{
    switch (key) {
        case KEY_SPACE: {
            inputs.p1shot = event;
            break;
        }
        case KEY_CONTROL: {
            inputs.p1shot = event;
            break;
        }
        case KEY_LEFT: {
            inputs.p1left = event;
            break;
        }
        case KEY_RIGHT: {
            inputs.p1right = event;
            break;
        }
        case KEY_5: {
            inputs.credit = event;
            break;
        }
        case KEY_1: {
            inputs.p1 = event;
            break;
        }
        case KEY_2: {
            inputs.p2 = event;
            break;
        }
        case KEY_ESCAPE: {
            i8080_state_ptr->halt_req = 1;
            break;
        }
        default: {
            /* Ignore all other keys */
            break;
        }
    }
}
