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

#include "x86.h"

.code32
.section .multiboot, "ax"

/* ====== Multiboot header  ====== */
.align 4
.global multiboot_header
multiboot_header:
.4byte 0x1badb002                 // Magic
.4byte 0x00000007                 // Flags (page align, mem info)
.4byte -(0x1badb002 + 0x00000007) // Checksum
.4byte 0x00000000 // MULTIBOOT_AOUT_KLUDGE
.4byte 0x00000000
.4byte 0x00000000
.4byte 0x00000000
.4byte 0x00000000 // End: MULTIBOOT_AOUT_KLUDGE
.4byte 0x00000000 // video_mode_type
.4byte 0x00000280 // video_width
.4byte 0x000001e0 // video_height : 640x480 32bpp
.4byte 0x00000020 // video_depth
/* ====== END: Multiboot header   ====== */

.global multiboot_ptr
.global multiboot_mjc
#if defined(__x86_64__)
multiboot_ptr:   .8byte 0
multiboot_mjc:   .8byte 0
#else
multiboot_ptr:   .4byte 0
multiboot_mjc:   .4byte 0
#endif

  /* ---------------------------------------------------------------------------*/
  /* Main Entry                                                                 */
  /* ---------------------------------------------------------------------------*/
.section .text, "ax"
.global _boot_start
_boot_start:
  cli
  mov $_boot_stack, %esp
  mov %ebx, multiboot_ptr
  mov %eax, multiboot_mjc

  /* ---------------------------------------------------------------------------*/
  /* Zero .bss section                                                          */
  /* ---------------------------------------------------------------------------*/
_boot_zero_bss:
  mov $__bss_start__, %eax
  mov $__bss_size__, %ecx
  mov $0x0, %ebx
1:
  mov %ebx, (%eax)
  inc %eax
  dec %ecx
  cmp %ebx, %ecx
  jne 1b

#if defined(__x86_64__)
  /* ****************************************************************************/
  /* x86_64: Enter Long Mode                                                    */
  /* ****************************************************************************/

  /* Step #1: setup page tables */
  call _boot_paging_init

  /* Step #2: enable PAE */
  mov $(1 << 5), %eax
  mov %eax, %cr4

  /* Step #3: Long Mode Enable (LME)  */
  movl $0xc0000080, %ecx /* MSR_EFER */
  rdmsr
  or $(1 << 8), %eax /* MSR_EFER_LME */
  wrmsr

  /* Step #4: Enable Paging */
  movl $((1 << 31) | (1 << 0)), %eax
  movl %eax, %cr0

  /* Now in 32-bit compatibility */

load_gdt:
  lgdt _boot_gdt_ptr
  mov $DATA_SELECTOR, %ax
  mov %ax, %ds
  mov %ax, %es
  mov %ax, %fs
  mov %ax, %gs
  mov %ax, %ss
  /* Step #5: enter 64-bit long mode */
  ljmp $CODE_SELECTOR, $_boot_start64

.code64
.global _boot_start64
_boot_start64:
  /* enable SSE */
  mov %cr0, %eax
  and $0xfffb, %ax
  or $0x2, %ax
  mov %eax, %cr0
  mov %cr4, %eax
  or $(3 << 9), %ax
  mov %eax, %cr4

#else /* !defined(__x86_64__) */
load_gdt:
  lgdt _boot_gdt_ptr
  mov $DATA_SELECTOR, %ax
  mov %ax, %ds
  mov %ax, %es
  mov %ax, %fs
  mov %ax, %gs
  mov %ax, %ss
  ljmp $CODE_SELECTOR, $load_gdt_continue
load_gdt_continue:
#endif

  call irq_init
  call main
  hlt
1:
  jmp 1b

#if defined(__x86_64__)
.code32
.section .text, "ax"
_boot_paging_init:
  /* Map the first 4GBs, 1GB pages */

  /* _boot_level3_table0[0..3] = 1GB Physical Page #0..3 */
.irp page,0,1,2,3
  mov $\page, %ecx
  mov $(\page * 1024*1024*1024), %eax
  or $(MMU_PRESENT | MMU_WRITE | MMU_PG_SIZE), %eax
  mov %eax, _boot_level3_table0(,%ecx,8)
.endr

  /*  _boot_level4_table[0] = &_boot_level3_table0[0] */
  mov $_boot_level3_table0, %eax
  or $(MMU_PRESENT | MMU_WRITE), %eax
  mov $0x00, %esi
  mov %eax, _boot_level4_table(%esi)

  /* set level4 page table base */
  mov $_boot_level4_table, %eax
  mov %eax, %cr3

  ret
#endif /* defined(__x86_64__) */

.section .data, "aw"
_boot_gdt:
/* Null segment */
.4byte 0x00000000
.4byte 0x00000000

#if defined(__x86_64__)
/* Code segment */
.4byte SEG_DESC64_W0
.4byte SEG_DESC64_W1(SEG_DESC_TYPE_CODE,PRIV_RING0,1,0,1)

/* Data segment */
.4byte SEG_DESC64_W0
.4byte SEG_DESC64_W1(SEG_DESC_TYPE_DATA,PRIV_RING0,0,0,0)

#else /* !defined(__x86_64__) */

/* Code segment : base 0, limit 0xfffff */
.4byte SEG_DESC32_W0(0xfffff,0)
.4byte SEG_DESC32_W1(0,SEG_DESC_TYPE_CODE,PRIV_RING0,0xfffff,1,1)

/* Data segment : base 0, limit 0xfffff */
.4byte SEG_DESC32_W0(0xffff,0)
.4byte SEG_DESC32_W1(0,SEG_DESC_TYPE_DATA,PRIV_RING0,0xffff,1,1)
#endif

_boot_gdt_ptr:
.2byte (3*8-1) // Null,Code,Data
#if defined(__x86_64__)
.8byte _boot_gdt
#else
.4byte _boot_gdt
#endif /* defined(__x86_64__) */

#if defined(__x86_64__)
.section .bss, "aw"

/* Level 4 Page table */
.global _boot_level4_table
.align (1024*4) /* must be 4KB aligned */
_boot_level4_table:
.space (512*8) /* 512 8-byte entries */

/* Level 3 Page Table */
.global _boot_level3_table0
.align (1024*4) /* must be 4KB aligned */
_boot_level3_table0:
.space (512*8) /* 512 8-byte entries */
/*_boot_level3_table1..511 not used */

#endif /* defined(__x86_64__) */

.section .bss, "aw"
.space 0x4000 /* 16KiB */
_boot_stack:
