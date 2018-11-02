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

#ifndef __X86_H__
#define __X86_H__

#define MK_FIELD(VAL,OFF,MASK) (((VAL) & (MASK)) << (OFF))

/* CPL, DPL, RPL*/
#define PRIV_RING0 0
#define PRIV_RING1 1
#define PRIV_RING2 2
#define PRIV_RING3 3

/* ==== Segment Selectors, described in Vol. 3A, 3-7 ==== */
#define SEG_SEL_TI_GDT 0
#define SEG_SEL_TI_LDT 1

#define SEG_SELECTOR(INDEX,TI,RPL)            \
    (MK_FIELD(RPL,   0,    0x3) |  \
     MK_FIELD(TI,    2,    0x1) |  \
     MK_FIELD(INDEX, 3, 0x1fff)    \
     )

/* ==== Segment Descriptors, described in Vol. 3A, 3-10 ==== */

/* Segment descriptor S (bit 4) and Type(bits 3...0) fields */
/* Code/Data: S=1 */
#define SEG_DESC_TYPE_DATA  (0b10010) /* Data Read/Write */
#define SEG_DESC_TYPE_CODE  (0b11010) /* Code Execute/Read */
/* System: S=0 */
#define SEG_DESC_TYPE_32BIT_IGATE (0b01110)

/* Segment Descriptors, 2 32-bit words */
#define SEG_DESC32_W0(LIMIT,BASE_ADDR)          \
    (MK_FIELD(LIMIT,      0, 0xffff) |          \
     MK_FIELD(BASE_ADDR, 16, 0xffff)            \
     )

#define SEG_DESC32_W1(BASE_ADDR,TYPE,DPL,LIMIT,DB,G)    \
    (MK_FIELD(BASE_ADDR,  0, 0xff) |                    \
     MK_FIELD(TYPE,       8, 0x1f) |                    \
     MK_FIELD(DPL,       13, 0x3)  |                    \
     MK_FIELD(1,         15, 0x1)  |                    \
     MK_FIELD(LIMIT,     16, 0xf)  |                    \
     MK_FIELD(DB,        22, 0x1)  |                    \
     MK_FIELD(G,         23, 0x1)  |                    \
     MK_FIELD(BASE_ADDR, 24, 0xff)                      \
     )

#define SEG_DESC64_W0 0x00000000

#define SEG_DESC64_W1(TYPE,DPL,L,DB,G)                  \
    (MK_FIELD(TYPE,       8, 0x1f) |                    \
     MK_FIELD(DPL,       13, 0x3)  |                    \
     MK_FIELD(1,         15, 0x1)  |                    \
     MK_FIELD(L,         21, 0x1)  |                    \
     MK_FIELD(DB,        22, 0x1)  |                    \
     MK_FIELD(G,         23, 0x1)                       \
     )

#define MMU_PRESENT (1 << 0)
#define MMU_WRITE   (1 << 1)
#define MMU_PG_SIZE (1 << 7)

#define NULL_SELECTOR  SEG_SELECTOR(0, SEG_SEL_TI_GDT, PRIV_RING0)
#define CODE_SELECTOR  SEG_SELECTOR(1, SEG_SEL_TI_GDT, PRIV_RING0)
#define DATA_SELECTOR  SEG_SELECTOR(2, SEG_SEL_TI_GDT, PRIV_RING0)

#define INTERRUPT_GATE 0x8e

#ifndef __ASSEMBLER__
#include <stdint.h>

#define pointer_cast(POINTER_TYPE,VALUE) (POINTER_TYPE)(uintptr_t)(VALUE)

static inline uint8_t inport8 (const uint16_t port)
{
    uint8_t val;
    asm volatile ("in %%dx, %%al" : "=a" (val) : "d" (port));
    return val;
}

static inline void outport8 (const uint16_t port, uint8_t val)
{
    asm volatile ("out %%al, %%dx" : /* no inputs */ : "a" (val), "d" (port));
}

static inline void irq_enable (void)
{
    asm volatile ("sti");
}

static inline void irq_disable (void)
{
    asm volatile ("cli");
}

static inline void set_msr(uint32_t msr_id, uint64_t msr_value)
{
    asm volatile ( "wrmsr" : : "c" (msr_id), "A" (msr_value) );
}

static inline void wrmsr(uint64_t msr, uint64_t value)
{
    uint32_t low = value & 0xFFFFFFFF;
    uint32_t high = value >> 32;
    asm volatile (
                  "wrmsr"
                  :
                  : "c"(msr), "a"(low), "d"(high)
                  );
}

static inline uint64_t get_msr (uint32_t msr)
{
    uint64_t val;
    asm volatile ( "rdmsr" : "=A" (val) : "c" (msr) );
    return val;
}

static inline uint64_t rdmsr(uint64_t msr)
{
    uint32_t low, high;
    asm volatile (
                  "rdmsr"
                  : "=a"(low), "=d"(high)
                  : "c"(msr)
                  );
    return ((uint64_t)high << 32) | low;
}

void show_cpu_info (void);

#endif /* __ASSEMBLER__ */

#endif /* __X86_H__ */
