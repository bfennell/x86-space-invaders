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

#ifndef __I8080_H__
#define __I8080_H__

#include <stdint.h>

#define DEVICE_IN  0
#define DEVICE_OUT 1

struct i8080_state;

typedef uint8_t (*i8080_io_fn_t)(const uint8_t port, const uint8_t byte, const int direction);
typedef int (*i8080_instr_fn_t)(struct i8080_state* state);

/* 7 6 5 4 3 2 1 0
   S Z I H - P - C
*/
typedef struct
{
    unsigned s:1;  /* =1 if result MSbit is set */
    unsigned z:1;  /* =1 if result is zero */
    unsigned p:1;  /* =1 if result has even parity */
    unsigned cy:1  /* =1 if result had a carry */;
    unsigned ac:1; /* =1 if result[3:0] had a carry */;
} flags_t;

typedef struct i8080_state
{
    uint8_t a;
    uint8_t b;
    uint8_t c;
    uint8_t d;
    uint8_t e;
    uint8_t h;
    uint8_t l;
    uint8_t i; /* interrupt enable */
    uint16_t sp;
    uint16_t pc;
    flags_t f;
    uint8_t* mem;
    int mem_sizeb;
    i8080_io_fn_t io_handler;
    i8080_instr_fn_t instr_func;
    unsigned irq_set_cnt;
    unsigned irq_clr_cnt;
    int halt_req;
} i8080_state_t;

i8080_state_t* i8080_init (i8080_state_t* state, uint8_t* ram, const int sizeb);
int i8080_exec (i8080_state_t* state);

void i8080_set_pc (i8080_state_t* state, uint16_t pc);
void i8080_set_io_handler (i8080_state_t* state, i8080_io_fn_t io_func);
void i8080_set_instr_handler (i8080_state_t* state, i8080_instr_fn_t instr_func);
void i8080_load_memory (i8080_state_t* state, const int offset, uint8_t* buffer, const int len);
void i8080_interrupt (i8080_state_t* state, uint8_t nnn);

#endif /*  __I8080_H__ */
