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

#include "i8080.h"
#include "stdio.h"
#include "bdos.h"

int bdos_entry (i8080_state_t* state)
{
    if (state->pc == 0x5) { /* BDOS entry point */
        switch (state->c) {
            case 0x2: { /* BDOS: C_WRITE */
                /* e = char to write */
                putchar (state->e);
                break;
            }
            case 0x9: { /* BDOS: C_WRITESTR */
                /* de = address of string */
                uint16_t de = (((uint8_t)state->d << 8 ) | ((uint8_t)state->e << 0));
                while (state->mem[de] != '$') {
                    putchar (state->mem[de]);
                    de++;
                }
                break;
            }
            default: {
                printf ("[error]: unknown BDOS function %02x\n", state->c);
                break;
            }
        }

        /* perform "ret" from BDOS "call" */
        state->pc = ((state->mem[state->sp + 1] << 8) | state->mem[state->sp]);
        state->sp += 2;

        return 0;

    } else if (state->pc == 0x0000) {
        state->halt_req = 1;
        return -1;
    }

    return 0;
}
