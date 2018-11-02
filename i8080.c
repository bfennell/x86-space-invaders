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
#include <stdlib.h>

#include "i8080.h"
#include "stdio.h"

/* #define TRACE_I8080 */
#if defined(TRACE_I8080)
#define i8080_TRACE(x) x;
#else
#define i8080_TRACE(x)
#endif

i8080_state_t* i8080_init (i8080_state_t* state, uint8_t* ram, const int sizeb)
{
    if (state != NULL) {
        memset (state, 0, sizeof(i8080_state_t));
    }

    state->mem = ram;
    state->mem_sizeb = sizeb;
    memset (state->mem, 0, sizeb);

    return state;
}

void i8080_set_pc (i8080_state_t* state, uint16_t pc)
{
    state->pc = pc;
}

void i8080_set_io_handler (i8080_state_t* state, i8080_io_fn_t io_func)
{
    state->io_handler = io_func;
}

void i8080_set_instr_handler (i8080_state_t* state, i8080_instr_fn_t instr_func)
{
    state->instr_func = instr_func;
}

static inline int i8080_parity (int8_t val)
{
    int i;
    int parity = 0;
    for (i = 0; i < 8; i++)
        parity += (val >> i);
    parity = ((parity & 1) == 0) ? 1 : 0;
    return parity;
}

static inline void i8080_update_flags (i8080_state_t* state, uint16_t result, int8_t dst, int8_t src)
{
    state->f.ac = ((dst ^ result ^ src) & 0x10) ? 1 : 0;
    state->f.s =  ((result & 0x80) == 0) ? 0 : 1;
    state->f.z =  ((result & 0xff) == 0) ? 1 : 0;
    state->f.p = i8080_parity (result & 0xff);
    state->f.cy =  ((result & 0x100) == 0) ? 0 : 1;
}

#if defined(TRACE_I8080)
static inline const char* reg2str (i8080_state_t* state, int8_t reg)
{
    switch (reg) {
        case 0: return "b";
        case 1: return "c";
        case 2: return "d";
        case 3: return "e";
        case 4: return "h";
        case 5: return "l";
        case 7: return "a";
        default: {
            printf ("[error] invalid register %02x\n", reg);
            state->halt_req = 1;
        }
    }
}
#endif

static inline uint8_t* reg_ptr (i8080_state_t* state, uint8_t reg)
{
    switch (reg) {
        case 0: return &state->b;
        case 1: return &state->c;
        case 2: return &state->d;
        case 3: return &state->e;
        case 4: return &state->h;
        case 5: return &state->l;
        case 7: return &state->a;
        default: {
            printf ("[error] invalid register %02x\n", reg);
            state->halt_req = 1;
        }
    }

    return &state->a;
}

static inline void movr2r (i8080_state_t* state)
{
    const uint8_t opcode = state->mem[state->pc];
    const uint8_t src_nr = (opcode & 0x7);
    const uint8_t dst_nr = ((opcode & 0x38) >> 3);
    uint8_t* src = reg_ptr (state, src_nr);
    uint8_t* dst = reg_ptr (state, dst_nr);

    i8080_TRACE(printf ("0x%04x: mov %s,%s\n", state->pc, reg2str(state, dst_nr), reg2str(state, src_nr)));
    *dst = *src;
    state->pc++;
}

static inline void movr2m (i8080_state_t* state, const uint16_t hl)
{
    const uint8_t opcode = state->mem[state->pc];
    const uint8_t src_nr = (opcode & 0x7);
    uint8_t* src = reg_ptr (state, src_nr);

    i8080_TRACE(printf ("0x%04x: mov m(0x%04x),%s\n", state->pc, hl, reg2str(state, src_nr)));
    state->mem[hl] = *src;
    state->pc++;
}

static inline void movm2r (i8080_state_t* state, const uint16_t hl)
{
    const uint8_t opcode = state->mem[state->pc];
    const uint8_t dst_nr = ((opcode & 0x38) >> 3);
    uint8_t* dst = reg_ptr (state, dst_nr);

    i8080_TRACE(printf ("0x%04x: mov %s,m(0x%04x)\n", state->pc, reg2str(state, dst_nr), hl));
    *dst = state->mem[hl];
    state->pc++;
}

static inline void mvi (i8080_state_t* state)
{
    const uint8_t opcode = state->mem[state->pc];
    const uint8_t dst_nr = ((opcode & 0x38) >> 3);
    const uint8_t byte = state->mem[state->pc+1];
    uint8_t* dst = reg_ptr (state, dst_nr);

    i8080_TRACE(printf ("0x%04x: mvi %s,0x%02x\n", state->pc, reg2str(state, dst_nr), byte));

    *dst = byte;
    state->pc += 2;
}

static inline void add (i8080_state_t* state)
{
    const uint8_t opcode = state->mem[state->pc];
    const uint8_t src_nr = (opcode & 0x7);
    uint8_t* src = reg_ptr (state, src_nr);
    uint16_t result;

    i8080_TRACE(printf ("0x%04x: add %s\n", state->pc, reg2str(state, src_nr)));
    result = state->a + *src;
    i8080_update_flags (state, result, state->a, *src);
    state->a = (result & 0xff);
    state->pc++;
}

static inline void adc (i8080_state_t* state)
{
    const uint8_t opcode = state->mem[state->pc];
    const uint8_t src_nr = (opcode & 0x7);
    uint8_t* src = reg_ptr (state, src_nr);
    uint16_t result;

    i8080_TRACE(printf ("0x%04x: adc %s\n", state->pc, reg2str(state, src_nr)));
    result = state->a + *src + state->f.cy;
    i8080_update_flags (state, result, state->a, (*src+state->f.cy));
    state->a = (result & 0xff);
    state->pc++;
}

static inline void sub (i8080_state_t* state)
{
    const uint8_t opcode = state->mem[state->pc];
    const uint8_t src_nr = (opcode & 0x7);
    uint8_t* src = reg_ptr (state, src_nr);
    uint16_t result;

    i8080_TRACE(printf ("0x%04x: sub %s\n", state->pc, reg2str(state, src_nr)));
    result = state->a - *src;
    i8080_update_flags (state, result, state->a, *src);
    state->a = (result & 0xff);
    state->pc++;
}

static inline void cmp (i8080_state_t* state)
{
    const uint8_t opcode = state->mem[state->pc];
    const uint8_t src_nr = (opcode & 0x7);
    uint8_t* src = reg_ptr (state, src_nr);
    uint16_t result;

    i8080_TRACE(printf ("0x%04x: cmp %s\n", state->pc, reg2str(state, src_nr)));
    result = state->a - *src;
    i8080_update_flags (state, result, state->a, *src);
    state->pc++;
}

static inline void sbb (i8080_state_t* state)
{
    const uint8_t opcode = state->mem[state->pc];
    const uint8_t src_nr = (opcode & 0x7);
    uint8_t* src = reg_ptr (state, src_nr);
    uint16_t result;

    i8080_TRACE(printf ("0x%04x: sbb %s\n", state->pc, reg2str(state, src_nr)));
    result = state->a - *src - state->f.cy;
    i8080_update_flags (state, result, state->a, (*src + state->f.cy));
    state->a = (result & 0xff);
    state->pc++;
}

static inline void inr (i8080_state_t* state)
{
    const uint8_t opcode = state->mem[state->pc];
    const uint8_t dst_nr = ((opcode & 0x38) >> 3);
    uint8_t* dst = reg_ptr (state, dst_nr);
    uint8_t cy = state->f.cy;
    uint16_t result;

    i8080_TRACE(printf ("0x%04x: inr %s\n", state->pc, reg2str(state, dst_nr)));
    result = *dst + 1;
    i8080_update_flags (state, result, *dst, 1);
    state->f.cy = cy;
    *dst = (result & 0xff);
    state->pc++;
}

static inline void dcr (i8080_state_t* state)
{
    const uint8_t opcode = state->mem[state->pc];
    const uint8_t dst_nr = ((opcode & 0x38) >> 3);
    uint8_t* dst = reg_ptr (state, dst_nr);
    uint8_t cy = state->f.cy;
    uint16_t result;

    i8080_TRACE(printf ("0x%04x: dcr %s\n", state->pc, reg2str(state, dst_nr)));
    result = *dst - 1;
    i8080_update_flags (state, result, *dst, 1);
    state->f.cy = cy;
    *dst = (result & 0xff);
    state->pc++;
}

static inline void call (i8080_state_t* state, uint16_t address)
{
    state->mem[state->sp - 1] = (((state->pc + 3) & 0xff00 ) >> 8);
    state->mem[state->sp - 2] = (((state->pc + 3) & 0x00ff ) >> 0);
    state->sp -= 2;
    state->pc = address;
}

static inline void rst (i8080_state_t* state)
{
    uint8_t nnn = ((state->mem[state->pc] >> 3) & 0x7);

    i8080_TRACE(printf ("0x%04x: rst %d\n", state->pc, nnn));

    state->mem[state->sp - 1] = (((state->pc + 1) & 0xff00 ) >> 8);
    state->mem[state->sp - 2] = (((state->pc + 1) & 0x00ff ) >> 0);
    state->sp -= 2;
    state->pc = (nnn * 8);
}

static inline void ret (i8080_state_t* state)
{
    state->pc = ((state->mem[state->sp + 1] << 8) | state->mem[state->sp]);
    state->sp += 2;
}

static inline void ana (i8080_state_t* state)
{
    const uint8_t opcode = state->mem[state->pc];
    const uint8_t src_nr = (opcode & 0x7);
    uint8_t* src = reg_ptr (state, src_nr);
    uint16_t result;

    i8080_TRACE(printf ("0x%04x: ana %s\n", state->pc, reg2str(state, src_nr)));
    result = state->a & *src;
    i8080_update_flags (state, result, state->a, *src);
    state->a = (result & 0xff);
    state->f.cy = 0;
    state->pc++;
}

static inline void xra (i8080_state_t* state)
{
    const uint8_t opcode = state->mem[state->pc];
    const uint8_t src_nr = (opcode & 0x7);
    uint8_t* src = reg_ptr (state, src_nr);
    uint16_t result;

    i8080_TRACE(printf ("0x%04x: xra %s\n", state->pc, reg2str(state, src_nr)));
    result = state->a ^ *src;
    i8080_update_flags (state, result, state->a, *src);
    state->a = (result & 0xff);
    state->f.cy = 0;
    state->f.ac = 0;
    state->pc++;
}

static inline void ora (i8080_state_t* state)
{
    const uint8_t opcode = state->mem[state->pc];
    const uint8_t src_nr = (opcode & 0x7);
    uint8_t* src = reg_ptr (state, src_nr);
    uint16_t result;

    i8080_TRACE(printf ("0x%04x: ora %s\n", state->pc, reg2str(state, src_nr)));
    result = state->a | *src;
    i8080_update_flags (state, result, state->a, *src);
    state->a = (result & 0xff);
    state->f.cy = 0;
    state->f.ac = 0;
    state->pc++;
}

int i8080_exec (i8080_state_t* state)
{
    uint16_t bc = ((uint8_t)state->b << 8 | (uint8_t)state->c);
    uint16_t de = ((uint8_t)state->d << 8 | (uint8_t)state->e);
    uint16_t hl = ((uint8_t)state->h << 8 | (uint8_t)state->l);

    if (state->halt_req) {
        return -1;
    }

    if (state->pc >= (state->mem_sizeb - 1)) {
        return -1;
    }

    /* check for special handling of this PC value */
    if (state->instr_func) {
        int status = state->instr_func (state);
        if (status != 0) {
            return status;
        }
    }

    switch (state->mem[state->pc]) {
        case 0x7f: case 0x78: case 0x79:
        case 0x7a: case 0x7b: case 0x7c:
        case 0x7d: {
            movr2r (state);
            break;
        }
        case 0x7e: movm2r (state, hl); break;
        case 0x0a: {
            i8080_TRACE(printf ("0x%04x: ldax b(%04x)\n", state->pc, bc));
            state->a = state->mem[bc];
            state->pc++;
            break;
        }
        case 0x07: {
            uint8_t b7 = state->a >> 7;
            i8080_TRACE(printf ("0x%04x: rlc\n", state->pc));
            state->a <<= 1;
            state->a |= b7;
            state->f.cy = b7;
            state->pc++;
            break;
        }
        case 0x0f: {
            uint8_t b0 = state->a & 1;
            i8080_TRACE(printf ("0x%04x: rrc\n", state->pc));
            state->a >>= 1;
            state->a |= (b0 << 7);
            state->f.cy = b0;
            state->pc++;
            break;
        }
        case 0x17: {
            uint8_t b7 = state->a >> 7;
            i8080_TRACE(printf ("0x%04x: ral\n", state->pc));
            state->a <<= 1;
            state->a |= state->f.cy;
            state->f.cy = b7;
            state->pc++;
            break;
        }
        case 0x1f: {
            uint8_t b0 = state->a & 1;
            i8080_TRACE(printf ("0x%04x: rar\n", state->pc));
            state->a >>= 1;
            state->a |= (state->f.cy << 7);
            state->f.cy = b0;
            state->pc++;
            break;
        }
        case 0x1a: {
            i8080_TRACE(printf ("0x%04x: ldax d(%04x)\n", state->pc, de));
            state->a = state->mem[de];
            state->pc++;
            break;
        }
        case 0x3a: {
            uint16_t word = (state->mem[state->pc+1] | state->mem[state->pc+2]<<8);
            i8080_TRACE(printf ("0x%04x: lda 0x%04x\n", state->pc, word));
            state->a = state->mem[word];
            state->pc += 3;
            break;
        }
        case 0x47: case 0x40: case 0x41:
        case 0x42: case 0x43: case 0x44:
        case 0x45: {
            movr2r (state);
            break;
        }
        case 0x46: movm2r (state, hl); break;
        case 0x4f: case 0x48: case 0x49:
        case 0x4a: case 0x4b: case 0x4c:
        case 0x4d: {
            movr2r (state);
            break;
        }
        case 0x4e: movm2r (state, hl); break;
        case 0x57: case 0x50: case 0x51:
        case 0x52: case 0x53: case 0x54:
        case 0x55: {
            movr2r (state);
            break;
        }
        case 0x56: movm2r (state, hl); break;
        case 0x5f: case 0x58: case 0x59:
        case 0x5a: case 0x5b: case 0x5c:
        case 0x5d: {
            movr2r (state);
            break;
        }
        case 0x5e: movm2r (state, hl); break;
        case 0x67: case 0x60: case 0x61:
        case 0x62: case 0x63: case 0x64:
        case 0x65: {
            movr2r (state);
            break;
        }
        case 0x66: movm2r (state, hl); break;
        case 0x6f: case 0x68: case 0x69:
        case 0x6a: case 0x6b: case 0x6c:
        case 0x6d: {
            movr2r (state);
            break;
        }
        case 0x6e: movm2r (state, hl); break;
        case 0x77: case 0x70: case 0x71:
        case 0x72: case 0x73: case 0x74:
        case 0x75: {
            movr2m (state, hl);
            break;
        }
        case 0x3e: case 0x06: case 0x0e:
        case 0x16: case 0x1e: case 0x26:
        case 0x2e: {
            mvi (state);
            break;
        }
        case 0x36: {
            uint8_t byte = state->mem[state->pc+1];
            i8080_TRACE(printf ("0x%04x: mvi m,0x%02x\n", state->pc, byte));
            state->mem[hl] = byte;
            state->pc += 2;
            break;
        }
        case 0x02: {
            i8080_TRACE(printf ("0x%04x: stax b\n", state->pc));
            state->mem[bc] = state->a;
            state->pc++;
            break;
        }
        case 0x12: {
            i8080_TRACE(printf ("0x%04x: stax d\n", state->pc));
            state->mem[de] = state->a;
            state->pc++;
            break;
        }
        case 0x32: {
            uint16_t word = (state->mem[state->pc+1] | state->mem[state->pc+2]<<8);
            i8080_TRACE(printf ("0x%04x: sta 0x%04x\n", state->pc, word));
            state->mem[word] = state->a;
            state->pc += 3;
            break;
        }
        case 0x01: {
            uint16_t word = (state->mem[state->pc+1] | state->mem[state->pc+2]<<8);
            i8080_TRACE(printf ("0x%04x: lxi b,0x%04x\n", state->pc, word));
            state->b = (word >> 8);
            state->c = (word & 0xff);
            state->pc += 3;
            break;
        }
        case 0x11: {
            uint16_t word = (state->mem[state->pc+1] | state->mem[state->pc+2]<<8);
            i8080_TRACE(printf ("0x%04x: lxi d,0x%04x\n", state->pc, word));
            state->d = (word >> 8);
            state->e = (word & 0xff);
            state->pc += 3;
            break;
        }
        case 0x21: {
            uint16_t word = (state->mem[state->pc+1] | state->mem[state->pc+2]<<8);
            i8080_TRACE(printf ("0x%04x: lxi h,0x%04x\n", state->pc, word));
            state->h = (word >> 8);
            state->l = (word & 0xff);
            state->pc += 3;
            break;
        }
        case 0x31: {
            uint16_t word = (state->mem[state->pc+1] | state->mem[state->pc+2]<<8);
            i8080_TRACE(printf ("0x%04x: lxi sp,0x%04x\n", state->pc, word));
            state->sp = word;
            state->pc += 3;
            break;
        }
        case 0x2a: {
            uint16_t addr = (state->mem[state->pc+1] | state->mem[state->pc+2]<<8);
            i8080_TRACE(printf ("0x%04x: lhld 0x%04x\n", state->pc, addr));
            state->l = state->mem[addr+0];
            state->h = state->mem[addr+1];
            state->pc += 3;
            break;
        }
        case 0x22: {
            uint16_t addr = (state->mem[state->pc+1] | state->mem[state->pc+2]<<8);
            i8080_TRACE(printf ("0x%04x: shld 0x%04x\n", state->pc, addr));
            state->mem[addr+0] = state->l;
            state->mem[addr+1] = state->h;
            state->pc += 3;
            break;
        }
        case 0xf9: {
            i8080_TRACE(printf ("0x%04x: sphl\n", state->pc));
            state->sp = hl;
            state->pc++;
            break;
        }
        case 0xeb: {
            i8080_TRACE(printf ("0x%04x: xchg\n", state->pc));
            state->h = ((de >> 8) & 0xff);
            state->l = (de & 0xff);
            state->d = ((hl >> 8) & 0xff);
            state->e = (hl & 0xff);
            state->pc++;
            break;
        }
        case 0xe3: {
            i8080_TRACE(printf ("0x%04x: xthl\n", state->pc));
            state->h = state->mem[state->sp+1];
            state->l = state->mem[state->sp];
            state->mem[state->sp+1] = (hl >> 8);
            state->mem[state->sp] = (hl & 0xff);
            state->pc++;
            break;
        }
        case 0x87: case 0x80: case 0x81:
        case 0x82: case 0x83: case 0x84:
        case 0x85: {
            add (state);
            break;
        }
        case 0x86: {
            uint16_t result;
            i8080_TRACE(printf ("0x%04x: add m\n", state->pc));
            result = state->a + state->mem[hl];
            i8080_update_flags (state, result, state->a, state->mem[hl]);
            state->a = result & 0xff;
            state->pc++;
            break;
        }
        case 0xc6: {
            uint16_t result;
            i8080_TRACE(printf ("0x%04x: adi 0x%02x\n", state->pc, state->mem[state->pc+1]));
            result = state->a + state->mem[state->pc+1];
            i8080_update_flags (state, result, state->a, state->mem[state->pc+1]);
            state->a = result & 0xff;
            state->pc += 2;
            break;
        }
        case 0x8f: case 0x88: case 0x89:
        case 0x8a: case 0x8b: case 0x8c:
        case 0x8d: {
            adc (state);
            break;
        }
        case 0x8e: {
            uint16_t result;
            i8080_TRACE(printf ("0x%04x: adc m\n", state->pc));
            result = state->a + state->mem[hl] + state->f.cy;
            i8080_update_flags (state, result, state->a, (state->mem[hl] + state->f.cy));
            state->a = result & 0xff;
            state->pc++;
            break;
        }
        case 0xce: {
            uint16_t result;
            i8080_TRACE(printf ("0x%04x: aci 0x%02x\n", state->pc, state->mem[state->pc+1]));
            result = state->a + state->mem[state->pc+1] + state->f.cy;
            i8080_update_flags (state, result, state->a, (state->mem[state->pc+1] + state->f.cy));
            state->a = result & 0xff;
            state->pc += 2;
            break;
        }
        case 0x97: case 0x90: case 0x91:
        case 0x92: case 0x93: case 0x94:
        case 0x95: {
            sub (state);
            break;
        }
        case 0x96: {
            uint16_t result;
            i8080_TRACE(printf ("0x%04x: sub m\n", state->pc));
            result = state->a - state->mem[hl];
            i8080_update_flags (state, result, state->a, state->mem[hl]);
            state->a = result & 0xff;
            state->pc++;
            break;
        }
        case 0xd6: {
            uint16_t result;
            i8080_TRACE(printf ("0x%04x: sui 0x%02x\n", state->pc, state->mem[state->pc+1]));
            result = state->a - state->mem[state->pc+1];
            i8080_update_flags (state, result, state->a, state->mem[state->pc+1]);
            state->a = result & 0xff;
            state->pc += 2;
            break;
        }
        case 0x9f: case 0x98: case 0x99:
        case 0x9a: case 0x9b: case 0x9c:
        case 0x9d: {
            sbb (state);
            break;
        }
        case 0x9e: {
            uint16_t result;
            i8080_TRACE(printf ("0x%04x: sbb m\n", state->pc));
            result = state->a - state->mem[hl] - state->f.cy;
            i8080_update_flags (state, result, state->a, (state->mem[hl] - state->f.cy));
            state->a = result & 0xff;
            state->pc++;
            break;
        }
        case 0xde: {
            uint16_t result;
            i8080_TRACE(printf ("0x%04x: sbi 0x%02x\n", state->pc, state->mem[state->pc+1]));
            result = state->a - state->mem[state->pc+1] - state->f.cy;
            i8080_update_flags (state, result, state->a, (state->mem[state->pc+1] - state->f.cy));
            state->a = result & 0xff;
            state->pc += 2;
            break;
        }
        case 0x09: {
            int32_t result;
            i8080_TRACE(printf ("0x%04x: dad b\n", state->pc));
            result = hl + bc;
            state->f.cy = ((result & 0x10000) == 0) ? 0 : 1;
            state->h = ((result & 0xff00) >> 8);
            state->l = ((result & 0x00ff) >> 0);
            state->pc++;
            break;
        }
        case 0x19: {
            int32_t result;
            i8080_TRACE(printf ("0x%04x: dad d\n", state->pc));
            result = hl + de;
            state->f.cy = ((result & 0x10000) == 0) ? 0 : 1;
            state->h = ((result & 0xff00) >> 8);
            state->l = ((result & 0x00ff) >> 0);
            state->pc++;
            break;
        }
        case 0x29: {
            int32_t result;
            i8080_TRACE(printf ("0x%04x: dad h\n", state->pc));
            result = hl + hl;
            state->f.cy = ((result & 0x10000) == 0) ? 0 : 1;
            state->h = ((result & 0xff00) >> 8);
            state->l = ((result & 0x00ff) >> 0);
            state->pc++;
            break;
        }
        case 0x39: {
            int32_t result;
            i8080_TRACE(printf ("0x%04x: dad sp\n", state->pc));
            result = hl + state->sp;
            state->f.cy = ((result & 0x10000) == 0) ? 0 : 1;
            state->h = ((result & 0xff00) >> 8);
            state->l = ((result & 0x00ff) >> 0);
            state->pc++;
            break;
        }
        case 0xf3: {
            i8080_TRACE(printf ("0x%04x: di\n", state->pc));
            state->i = 0;
            state->pc++;
            break;
        }
        case 0xfb: {
            i8080_TRACE(printf ("0x%04x: ei\n", state->pc));
            state->i = 1;
            state->pc++;
            break;
        }
        case 0x00: {
            i8080_TRACE(printf ("0x%04x: nop\n", state->pc));
            state->pc++;
            break;
        }
        case 0x76: {
            i8080_TRACE(printf ("0x%04x: hlt\n", state->pc));
            printf("HLT\n");
            return -1;
            break;
        }
        case 0x3c: case 0x04: case 0x0c:
        case 0x14: case 0x1c: case 0x24:
        case 0x2c: {
            inr (state);
            break;
        }
        case 0x34: {
            uint16_t result;
            uint8_t cy = state->f.cy;
            i8080_TRACE(printf ("0x%04x: inr m\n", state->pc));
            result = state->mem[hl] + 1;
            i8080_update_flags (state, result, state->mem[hl], 1);
            state->f.cy = cy;
            state->mem[hl] = result & 0xff;
            state->pc++;
            break;
        }
        case 0x3d: case 0x05: case 0x0d:
        case 0x15: case 0x1d: case 0x25:
        case 0x2d: {
            dcr (state);
            break;
        }
        case 0x35: {
            uint16_t result;
            uint8_t cy = state->f.cy;
            i8080_TRACE(printf ("0x%04x: dcr m\n", state->pc));
            result = state->mem[hl] - 1;
            i8080_update_flags (state, result, state->mem[hl], 1);
            state->f.cy = cy;
            state->mem[hl] = result & 0xff;
            state->pc++;
            break;
        }
        case 0x03: {
            i8080_TRACE(printf ("0x%04x: inx b\n", state->pc));
            bc++;
            state->b = ((bc & 0xff00) >> 8);
            state->c = ((bc & 0x00ff) >> 0);
            state->pc++;
            break;
        }
        case 0x13: {
            i8080_TRACE(printf ("0x%04x: inx d\n", state->pc));
            de++;
            state->d = ((de & 0xff00) >> 8);
            state->e = ((de & 0x00ff) >> 0);
            state->pc++;
            break;
        }
        case 0x23: {
            i8080_TRACE(printf ("0x%04x: inx h\n", state->pc));
            hl++;
            state->h = ((hl & 0xff00) >> 8);
            state->l = ((hl & 0x00ff) >> 0);
            state->pc++;
            break;
        }
        case 0x33: {
            i8080_TRACE(printf ("0x%04x: inx sp\n", state->pc));
            state->sp++;
            state->pc++;
            break;
        }
        case 0x0b: {
            i8080_TRACE(printf ("0x%04x: dcx b\n", state->pc));
            bc--;
            state->b = ((bc & 0xff00) >> 8);
            state->c = ((bc & 0x00ff) >> 0);
            state->pc++;
            break;
        }
        case 0x1b: {
            i8080_TRACE(printf ("0x%04x: dcx d\n", state->pc));
            de--;
            state->d = ((de & 0xff00) >> 8);
            state->e = ((de & 0x00ff) >> 0);
            state->pc++;
            break;
        }
        case 0x27: {
            uint8_t lnibble;
            uint8_t hnibble;
            int cy = state->f.cy;
            int ac;

            i8080_TRACE(printf ("0x%04x: daa\n", state->pc));

            lnibble = (state->a & 0xf);
            if ((lnibble > 9) || state->f.ac) {
                uint16_t result = (state->a + 6) & 0xff;
                i8080_update_flags (state, result, state->a, 6);
                state->f.ac = 1;
                state->a = (result & 0xff);
            } else {
                state->f.ac = 0;
            }
            ac = state->f.ac;

            hnibble = ((state->a >> 4) & 0xf);
            if ((hnibble > 9) || cy) {
                uint16_t result = (state->a + 0x60);
                i8080_update_flags (state, result, state->a, 0x60);
                state->f.cy = 1;
                state->a = (result & 0xff);
            } else {
                state->f.cy = 0;
            }
            state->f.ac = ac;

            state->pc++;
            break;
        }
        case 0x2b: {
            i8080_TRACE(printf ("0x%04x: dcx h\n", state->pc));
            hl--;
            state->h = ((hl & 0xff00) >> 8);
            state->l = ((hl & 0x00ff) >> 0);
            state->pc++;
            break;
        }
        case 0x3b: {
            i8080_TRACE(printf ("0x%04x: dcx sp\n", state->pc));
            state->sp--;
            state->pc++;
            break;
        }
        case 0x2f: {
            i8080_TRACE(printf ("0x%04x: cma\n", state->pc));
            state->a = ~state->a;
            state->pc++;
            break;
        }
        case 0x37: {
            i8080_TRACE(printf ("0x%04x: stc\n", state->pc));
            state->f.cy = 1;
            state->pc++;
            break;
        }
        case 0x3f: {
            i8080_TRACE(printf ("0x%04x: cmc\n", state->pc));
            state->f.cy = (~state->f.cy & 1);
            state->pc++;
            break;
        }
        case 0xa7: case 0xa0: case 0xa1:
        case 0xa2: case 0xa3: case 0xa4:
        case 0xa5: {
            ana (state);
            break;
        }
        case 0xa6: {
            uint16_t result;
            i8080_TRACE(printf ("0x%04x: ana m(0x%04x)\n", state->pc, hl));
            result = state->a & state->mem[hl];
            i8080_update_flags (state, result, state->a, state->mem[hl]);
            state->a = (result & 0xff);
            state->f.cy = 0;
            state->pc++;
            break;
        }
        case 0xe6: {
            uint16_t result;
            i8080_TRACE(printf ("0x%04x: ani 0x%02x\n", state->pc, state->mem[state->pc+1]));
            result = state->a & state->mem[state->pc+1];
            i8080_update_flags (state, result, state->a, state->mem[state->pc+1]);
            state->a = (result & 0xff);
            state->f.cy = 0;
            state->f.ac = 0;
            state->pc += 2;
            break;
        }
        case 0xaf: case 0xa8: case 0xa9:
        case 0xaa: case 0xab: case 0xac:
        case 0xad: {
            xra (state);
            break;
        }
        case 0xae: {
            uint16_t result;
            i8080_TRACE(printf ("0x%04x: xra m(0x%04x)\n", state->pc, hl));
            result = state->a ^ state->mem[hl];
            i8080_update_flags (state, result, state->a, state->mem[hl]);
            state->a = (result & 0xff);
            state->f.cy = 0;
            state->f.ac = 0;
            state->pc++;
            break;
        }
        case 0xee: {
            uint16_t result;
            i8080_TRACE(printf ("0x%04x: xri 0x%02x\n", state->pc, state->mem[state->pc+1]));
            result = state->a ^ state->mem[state->pc+1];
            i8080_update_flags (state, result, state->a, state->mem[state->pc+1]);
            state->a = (result & 0xff);
            state->f.cy = 0;
            state->f.ac = 0;
            state->pc += 2;
            break;
        }
        case 0xb7: case 0xb0: case 0xb1:
        case 0xb2: case 0xb3: case 0xb4:
        case 0xb5: {
            ora (state);
            break;
        }
        case 0xb6: {
            uint16_t result;
            i8080_TRACE(printf ("0x%04x: ora m(0x%04x)\n", state->pc, hl));
            result = state->a | state->mem[hl];
            i8080_update_flags (state, result, state->a, state->mem[hl]);
            state->a = (result & 0xff);
            state->f.cy = 0;
            state->f.ac = 0;
            state->pc++;
            break;
        }
        case 0xf6: {
            uint16_t result;
            i8080_TRACE(printf ("0x%04x: ori 0x%02x\n", state->pc, state->mem[state->pc+1]));
            result = state->a | state->mem[state->pc+1];
            i8080_update_flags (state, result, state->a, state->mem[state->pc+1]);
            state->a = (result & 0xff);
            state->f.cy = 0;
            state->f.ac = 0;
            state->pc += 2;
            break;
        }
        case 0xbf: case 0xb8: case 0xb9:
        case 0xba: case 0xbb: case 0xbc:
        case 0xbd: {
            cmp (state);
            break;
        }
        case 0xbe: {
            uint16_t result;
            i8080_TRACE(printf ("0x%04x: cmp m(%04x)\n", state->pc, hl));
            result = state->a - state->mem[hl];
            i8080_update_flags (state, result, state->a, state->mem[hl]);
            state->pc++;
            break;
        }
        case 0xfe: {
            uint16_t result;
            i8080_TRACE(printf ("0x%04x: cpi 0x%02x\n", state->pc, state->mem[state->pc+1]));
            result = state->a - state->mem[state->pc+1];
            i8080_update_flags (state, result, state->a, state->mem[state->pc+1]);
            state->pc += 2;
            break;
        }
        case 0xc3: {
            uint16_t address = (state->mem[state->pc+1] | (state->mem[state->pc+2] << 8));
            i8080_TRACE(printf ("0x%04x: jmp 0x%04x\n", state->pc, address));
            state->pc = address;
            break;
        }
        case 0xc2: {
            uint16_t address = (state->mem[state->pc+1] | (state->mem[state->pc+2] << 8));
            i8080_TRACE(printf ("0x%04x: jnz 0x%04x\n", state->pc, address));
            if (state->f.z == 0)
                state->pc = address;
            else
                state->pc += 3;
            break;
        }
        case 0xca: {
            uint16_t address = (state->mem[state->pc+1] | (state->mem[state->pc+2] << 8));
            i8080_TRACE(printf ("0x%04x: jz 0x%04x\n", state->pc, address));
            if (state->f.z == 1)
                state->pc = address;
            else
                state->pc += 3;
            break;
        }
        case 0xd2: {
            uint16_t address = (state->mem[state->pc+1] | (state->mem[state->pc+2] << 8));
            i8080_TRACE(printf ("0x%04x: jnc 0x%04x\n", state->pc, address));
            if (state->f.cy == 0)
                state->pc = address;
            else
                state->pc += 3;
            break;
        }
        case 0xda: {
            uint16_t address = (state->mem[state->pc+1] | (state->mem[state->pc+2] << 8));
            i8080_TRACE(printf ("0x%04x: jc 0x%04x\n", state->pc, address));
            if (state->f.cy == 1)
                state->pc = address;
            else
                state->pc += 3;
            break;
        }
        case 0xe2: {
            uint16_t address = (state->mem[state->pc+1] | (state->mem[state->pc+2] << 8));
            i8080_TRACE(printf ("0x%04x: jpo 0x%04x\n", state->pc, address));
            if (state->f.p == 0)
                state->pc = address;
            else
                state->pc += 3;
            break;
        }
        case 0xea: {
            uint16_t address = (state->mem[state->pc+1] | (state->mem[state->pc+2] << 8));
            i8080_TRACE(printf ("0x%04x: jpe 0x%04x\n", state->pc, address));
            if (state->f.p == 1)
                state->pc = address;
            else
                state->pc += 3;
            break;
        }
        case 0xf2: {
            uint16_t address = (state->mem[state->pc+1] | (state->mem[state->pc+2] << 8));
            i8080_TRACE(printf ("0x%04x: jp 0x%04x\n", state->pc, address));
            if (state->f.s == 0)
                state->pc = address;
            else
                state->pc += 3;
            break;
        }
        case 0xfa: {
            uint16_t address = (state->mem[state->pc+1] | (state->mem[state->pc+2] << 8));
            i8080_TRACE(printf ("0x%04x: jm 0x%04x\n", state->pc, address));
            if (state->f.s == 1)
                state->pc = address;
            else
                state->pc += 3;
            break;
        }
        case 0xe9: {
            i8080_TRACE(printf ("0x%04x: pchl\n", state->pc));
            state->pc = hl;
            break;
        }
        case 0xcd: {
            uint16_t address = (state->mem[state->pc+1] | (state->mem[state->pc+2] << 8));
            i8080_TRACE(printf ("0x%04x: call 0x%04x\n", state->pc, address));
            call (state, address);
            break;
        }
        case 0xc4: {
            uint16_t address = (state->mem[state->pc+1] | (state->mem[state->pc+2] << 8));
            i8080_TRACE(printf ("0x%04x: cnz 0x%04x\n", state->pc, address));
            if (state->f.z == 0)
                call (state, address);
            else
                state->pc += 3;
            break;
        }
        case 0xcc: {
            uint16_t address = (state->mem[state->pc+1] | (state->mem[state->pc+2] << 8));
            i8080_TRACE(printf ("0x%04x: cz 0x%04x\n", state->pc, address));
            if (state->f.z == 1)
                call (state, address);
            else
                state->pc += 3;
            break;
        }
        case 0xd4: {
            uint16_t address = (state->mem[state->pc+1] | (state->mem[state->pc+2] << 8));
            i8080_TRACE(printf ("0x%04x: cnc 0x%04x\n", state->pc, address));
            if (state->f.cy == 0)
                call (state, address);
            else
                state->pc += 3;
            break;
        }
        case 0xdc: {
            uint16_t address = (state->mem[state->pc+1] | (state->mem[state->pc+2] << 8));
            i8080_TRACE(printf ("0x%04x: cc 0x%04x\n", state->pc, address));
            if (state->f.cy == 1)
                call (state, address);
            else
                state->pc += 3;
            break;
        }
        case 0xe4: {
            uint16_t address = (state->mem[state->pc+1] | (state->mem[state->pc+2] << 8));
            i8080_TRACE(printf ("0x%04x: cpo 0x%04x\n", state->pc, address));
            if (state->f.p == 0)
                call (state, address);
            else
                state->pc += 3;
            break;
        }
        case 0xec: {
            uint16_t address = (state->mem[state->pc+1] | (state->mem[state->pc+2] << 8));
            i8080_TRACE(printf ("0x%04x: cpe 0x%04x\n", state->pc, address));
            if (state->f.p == 1)
                call (state, address);
            else
                state->pc += 3;
            break;
        }
        case 0xf4: {
            uint16_t address = (state->mem[state->pc+1] | (state->mem[state->pc+2] << 8));
            i8080_TRACE(printf ("0x%04x: cp 0x%04x\n", state->pc, address));
            if (state->f.s == 0)
                call (state, address);
            else
                state->pc += 3;
            break;
        }
        case 0xfc: {
            uint16_t address = (state->mem[state->pc+1] | (state->mem[state->pc+2] << 8));
            i8080_TRACE(printf ("0x%04x: cm 0x%04x\n", state->pc, address));
            if (state->f.s == 1)
                call (state, address);
            else
                state->pc += 3;
            break;
        }
        case 0xc9: {
            i8080_TRACE(printf ("0x%04x: ret\n", state->pc));
            ret (state);
            break;
        }
        case 0xc0: {
            i8080_TRACE(printf ("0x%04x: rnz\n", state->pc));
            if (state->f.z == 0)
                ret (state);
            else
                state->pc++;
            break;
        }
        case 0xc8: {
            i8080_TRACE(printf ("0x%04x: rz\n", state->pc));
            if (state->f.z == 1)
                ret (state);
            else
                state->pc++;
            break;
        }
        case 0xd0: {
            i8080_TRACE(printf ("0x%04x: rnc\n", state->pc));
            if (state->f.cy == 0)
                ret (state);
            else
                state->pc++;
            break;
        }
        case 0xd8: {
            i8080_TRACE(printf ("0x%04x: rc\n", state->pc));
            if (state->f.cy == 1)
                ret (state);
            else
                state->pc++;
            break;
        }
        case 0xe0: {
            i8080_TRACE(printf ("0x%04x: rpo\n", state->pc));
            if (state->f.p == 0)
                ret (state);
            else
                state->pc++;
            break;
        }
        case 0xe8: {
            i8080_TRACE(printf ("0x%04x: rpe\n", state->pc));
            if (state->f.p == 1)
                ret (state);
            else
                state->pc++;
            break;
        }
        case 0xf0: {
            i8080_TRACE(printf ("0x%04x: rp\n", state->pc));
            if (state->f.s == 0)
                ret (state);
            else
                state->pc++;
            break;
        }
        case 0xf8: {
            i8080_TRACE(printf ("0x%04x: rm\n", state->pc));
            if (state->f.s == 1)
                ret (state);
            else
                state->pc++;
            break;
        }
        case 0xc7: case 0xcf: case 0xd7:
        case 0xdf: case 0xe7: case 0xef:
        case 0xf7: case 0xff: {
            rst (state);
            break;
        }
        case 0xc5: {
            i8080_TRACE(printf ("0x%04x: push b\n", state->pc));
            state->mem[state->sp - 1] = state->b;
            state->mem[state->sp - 2] = state->c;
            state->sp -= 2;
            state->pc++;
            break;
        }
        case 0xd5: {
            i8080_TRACE(printf ("0x%04x: push d\n", state->pc));
            state->mem[state->sp - 1] = state->d;
            state->mem[state->sp - 2] = state->e;
            state->sp -= 2;
            state->pc++;
            break;
        }
        case 0xe5: {
            i8080_TRACE(printf ("0x%04x: push h\n", state->pc));
            state->mem[state->sp - 1] = state->h;
            state->mem[state->sp - 2] = state->l;
            state->sp -= 2;
            state->pc++;
            break;
        }
        case 0xf5: {
            i8080_TRACE(printf ("0x%04x: push psw\n", state->pc));
            state->mem[state->sp - 1] = state->a;
            state->mem[state->sp - 2] = ((state->f.cy << 0) | (1 << 1) |
                                         (state->f.p  << 2) | (0 << 3) |
                                         (state->f.ac << 4) | (0 << 5) |
                                         (state->f.z  << 6) | (state->f.s << 7));
            state->sp -= 2;
            state->pc++;
            break;
        }
        case 0xc1: {
            i8080_TRACE(printf ("0x%04x: pop b\n", state->pc));
            state->c = state->mem[state->sp];
            state->b = state->mem[state->sp + 1];
            state->sp += 2;
            state->pc++;
            break;
        }
        case 0xd1: {
            i8080_TRACE(printf ("0x%04x: pop d\n", state->pc));
            state->e = state->mem[state->sp];
            state->d = state->mem[state->sp + 1];
            state->sp += 2;
            state->pc++;
            break;
        }
        case 0xe1: {
            i8080_TRACE(printf ("0x%04x: pop h\n", state->pc));
            state->l = state->mem[state->sp];
            state->h = state->mem[state->sp + 1];
            state->sp += 2;
            state->pc++;
            break;
        }
        case 0xf1: {
            i8080_TRACE(printf ("0x%04x: pop psw\n", state->pc));
            state->a = state->mem[state->sp + 1];
            state->f.cy = ((state->mem[state->sp] >> 0) & 1);
            state->f.p  = ((state->mem[state->sp] >> 2) & 1);
            state->f.ac = ((state->mem[state->sp] >> 4) & 1);
            state->f.z  = ((state->mem[state->sp] >> 6) & 1);
            state->f.s  = ((state->mem[state->sp] >> 7) & 1);
            state->sp += 2;
            state->pc++;
            break;
        }
        case 0xdb: {
            uint8_t port = state->mem[state->pc+1];
            i8080_TRACE(printf ("0x%04x: in 0x%02x\n", state->pc, port));

            if (state->io_handler) {
                state->a = state->io_handler (port, 0xee, DEVICE_IN);
            }

            state->pc += 2;
            break;
        }
        case 0xd3: {
            uint8_t port = state->mem[state->pc+1];
            i8080_TRACE(printf ("0x%04x: out 0x%02x\n", state->pc, port));

            if (state->io_handler) {
                state->io_handler (port, state->a, DEVICE_OUT);
            }

            state->pc += 2;
            break;
        }
        default: {
            printf ("Error: [unknown opcode] PC: %04x Opcode: %02x\n", state->pc, state->mem[state->pc]);
            return -1;
        }
    }

    return 0;
}

void i8080_interrupt (i8080_state_t* state, uint8_t nnn)
{
    if (state->i) {
        i8080_TRACE(printf ("0x%04x: <interrupt> 0x%02x\n", state->pc, nnn));

        /* same as RST instruction */
        state->mem[state->sp - 1] = ((state->pc & 0xeff00 ) >> 8);
        state->mem[state->sp - 2] = ((state->pc & 0x00ff ) >> 0);
        state->i = 0; /* disable interrupts */
        state->sp -= 2;
        state->pc = (nnn * 8);
    }
}

void i8080_load_memory (i8080_state_t* state, const int offset, uint8_t* buffer, const int len)
{
    int size = (len > (state->mem_sizeb - offset)) ? (state->mem_sizeb - offset) : len;

    for (int i = 0; i < size; ++i) {
        state->mem[offset + i] = buffer[i];
    }
}
