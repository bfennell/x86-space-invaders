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

#include "stdio.h"
#include "x86.h"

typedef struct {
    unsigned eax;
    unsigned ebx;
    unsigned edx;
    unsigned ecx;
} cpuid_t;

static inline cpuid_t cpuid (uint32_t eax)
{
    cpuid_t regs;
    asm volatile ("cpuid\n\t"
                  : "=a" (regs.eax), "=b" (regs.ebx), "=c" (regs.ecx), "=d" (regs.edx)
                  : "a" (eax), "c" (0)
                  );

    return regs;
}

void show_cpu_info (void)
{
    union {
        char str[16*4+1]; /* 3 words, 4 bytes each + '\0' */
        uint32_t val[16];
    } buffer;

    cpuid_t regs = cpuid (0x00);

    int i = 0;
    buffer.val[i++] = regs.ebx;
    buffer.val[i++] = regs.edx;
    buffer.val[i++] = regs.ecx;
    buffer.str[i*4] = '\0';

    /* processor vendor */
    printf ("%s : ", buffer.str);


    /* processor brand */
    i = 0;
    regs = cpuid (0x80000002);
    buffer.val[i++] = regs.eax;
    buffer.val[i++] = regs.ebx;
    buffer.val[i++] = regs.ecx;
    buffer.val[i++] = regs.edx;
    regs = cpuid (0x80000003);
    buffer.val[i++] = regs.eax;
    buffer.val[i++] = regs.ebx;
    buffer.val[i++] = regs.ecx;
    buffer.val[i++] = regs.edx;
    regs = cpuid (0x80000004);
    buffer.val[i++] = regs.eax;
    buffer.val[i++] = regs.ebx;
    buffer.val[i++] = regs.ecx;
    buffer.val[i++] = regs.edx;
    buffer.str[i*4] = '\0';

    printf ("%s\n", buffer.str);

    regs = cpuid (0x80000001);
    if ((regs.edx & (1 << 29)) != 0) {
        printf ("(x86_64 ");
    } else {
        printf ("(i386 ");
    }


    uint64_t efer = get_msr(0xc0000080);
    if (efer & (1 << 10)) {
        printf ("64-bit)\n");
    } else {
        printf ("32-bit)\n");
    }
}
