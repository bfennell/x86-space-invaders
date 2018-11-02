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
#include "asm.h"

#define NR_IRQS 48

.section .text, "ax"

/* Interrupts 0... 31
  00  Division by zero
  01  Single-step interrupt (see trap flag)
  02  NMI
  03  Breakpoint (used by debuggers)
  04  Overflow
  05  Bounds
  06  Invalid Opcode
  07  Coprocessor not available
  08  Double fault
  09  Coprocessor Segment Overrun (386 or earlier only)
  10  Invalid Task State Segment
  11  Segment not present
  12  Stack Fault
  13  General protection fault
  14  Page fault
  15  reserved
  16  Math Fault
  17  Alignment Check
  18  Machine Check
  19  SIMD Floating-Point Exception
  20  Virtualization Exception
  21  Control Protection Exception
  22  Reserved
  23  Reserved
  24  Reserved
  25  Reserved
  26  Reserved
  27  Reserved
  28  Reserved
  29  Reserved
  30  Reserved
  31  Reserved
  */
.irp irq_nr,0,1,2,3,4,5,6,7,8,9,10,11,12,13,14,15,16,17,18,19,20,21,22,23,24,25,26,27,28,29,30,31
.global irq\irq_nr
irq\irq_nr:
  cli
  call_function2 printf $irq_unexpected_msg $\irq_nr
  hlt
.endr

/* Interrupts 34 ... 53
  34  Cascade (used internally by the two PICs. never raised)
  35  COM2 (if enabled)
  36  COM1 (if enabled)
  37  LPT2 (if enabled)
  38  Floppy Disk
  39  LPT1 / Unreliable "spurious" interrupt (usually)
  40  CMOS real-time clock (if enabled)
  41  Free for peripherals / legacy SCSI / NIC
  42  Free for peripherals / SCSI / NIC
  43  Free for peripherals / SCSI / NIC
  44  PS2 Mouse
  45  FPU / Coprocessor / Inter-processor
  46  Primary ATA Hard Disk
  47  Secondary ATA Hard Disk
  */
.irp irq_nr,34,35,36,37,38,39,40,41,42,43,44,45,46,47
.global irq\irq_nr
irq\irq_nr:
  cli
  call_function2 printf $irq_unexpected_msg $\irq_nr
  hlt
.endr

/* macro to install interrupt handlers for expected interrupts */
.macro IRQ_HANDLER irq_nr handler
.global irq\irq_nr
irq\irq_nr:
  cli
  pusha

  /* PIC master, clear interrupt */
  mov $0x20, %ax
  mov $0x20, %dx
  out %al, %dx
  call \handler

  popa
  sti

#if defined(__x86_64__)
  iretq
#else
  iret
#endif
.endm

/*
  32  Programmable Interrupt Timer Interrupt
  33  Keyboard Interrupt
*/
IRQ_HANDLER 32 timer_irq_handler
IRQ_HANDLER 33 keyboard_irq_handler

/* load the interrupt descriptor */
.global load_idt
load_idt:
  mov $idt_descriptor, %eax
  lidt (%eax)
  ret

#if defined(__x86_64__)
/* write an idt entry */
.global load_idt_entry
load_idt_entry:
  mov 0x8(%rsp), %rbx /* irq address */
  mov 0x10(%rsp), %rax /* index */
  shl $0x4, %rax  /* 16-byte entries */
  /* 0x00: address low(15...0) */
  mov $0x0, %rsi
  mov %bx, idt_table(%rsi,%rax)
  /* 0x02: selector */
  mov $0x2, %rsi
  mov $CODE_SELECTOR, %cx
  mov %cl, idt_table(%rsi,%rax)
  /* 0x04: zero */
  mov $0x4, %rsi
  movb $0x00, idt_table(%rsi,%rax)
  /* 0x05: attributes */
  mov $0x5, %rsi
  mov $INTERRUPT_GATE, %cx
  mov %cl, idt_table(%rsi,%rax)
  /* 0x06: address middle(31..16) */
  mov $0x6, %rsi
  shr $0x10, %rbx
  mov %bx, idt_table(%rsi,%rax)
  /* 0x08: address high(63..32) */
  mov $0x8, %rsi
  shr $0x10, %rbx
  mov %bx, idt_table(%rsi,%rax)
  ret
#else
/* write an idt entry */
.global load_idt_entry
load_idt_entry:
  mov 0x4(%esp), %ebx /* irq address */
  mov 0x8(%esp), %eax /* index */
  /* 0x00: address low */
  mov $0x0, %esi
  mov %bx, idt_table(%esi,%eax,8)
  /* 0x02: selector */
  mov $0x2, %esi
  mov $CODE_SELECTOR, %cx
  mov %cl, idt_table(%esi,%eax,8)
  /* 0x04: zero */
  mov $0x4, %esi
  movb $0x00, idt_table(%esi,%eax,8)
  /* 0x05: attributes */
  mov $0x5, %esi
  mov $INTERRUPT_GATE, %cx
  mov %cl, idt_table(%esi,%eax,8)
  /* 0x06: address high */
  mov $0x6, %esi
  shr $0x10, %ebx
  mov %bx, idt_table(%esi,%eax,8)
  ret
#endif

/* initialise idt */
.global irq_init
irq_init:
  pusha

  /* interrupts 0 ... 15 */
.irp irq_nr,0,1,2,3,4,5,6,7,8,9,10,11,12,13,14,15
  push $\irq_nr
  push $irq\irq_nr
  call load_idt_entry
#if defined(__x86_64__)
  add $0x10, %rsp
#else
  add $0x08, %esp
#endif
.endr

  /* interrupts 16 ... 31 */
.irp irq_nr,16,17,18,19,20,21,22,23,24,25,26,27,28,29,30,31
  push $\irq_nr
  push $irq\irq_nr
  call load_idt_entry
#if defined(__x86_64__)
  add $0x10, %rsp
#else
  add $0x08, %esp
#endif
.endr

  /* interrupts 32 ... 47 */
.irp irq_nr,32,33,34,35,36,37,38,39,40,41,42,43,44,45,46,47
  push $\irq_nr
  push $irq\irq_nr
  call load_idt_entry
#if defined(__x86_64__)
  add $0x10, %rsp
#else
  add $0x08, %esp
#endif
.endr

  /* load idt */
  call load_idt

  /* IRQs on the Master/Slave PICs must be remapped above #31 */
  mov $0x20, %dx
  mov $0x11, %al
  out %al, %dx
  mov $0xa0, %dx
  mov $0x11, %al
  out %al, %dx
  mov $0x21, %dx
  mov $0x20, %al
  out %al, %dx
  mov $0xa1, %dx
  mov $0x28, %al
  out %al, %dx
  mov $0x21, %dx
  mov $0x04, %al
  out %al, %dx
  mov $0xa1, %dx
  mov $0x02, %al
  out %al, %dx
  mov $0x21, %dx
  mov $0x01, %al
  out %al, %dx
  mov $0xa1, %dx
  mov $0x01, %al
  out %al, %dx
  mov $0x21, %dx
  mov $0x0, %al
  out %al, %dx
  mov $0xa1, %dx
  mov $0x0, %al
  out %al, %dx
  popa
  ret

.section .bss, "aw"
idt_table:
#if defined(__x86_64__)
.space (16*NR_IRQS)
#else
.space (8*NR_IRQS)
#endif

.section .data, "aw"
idt_descriptor:
#if defined(__x86_64__)
.2byte (NR_IRQS * 16 - 1)
.8byte idt_table
#else
.2byte (NR_IRQS * 8 - 1)
.4byte idt_table
#endif

irq_unexpected_msg: .asciz "[error]: unexpected interrupt %08x\nHLT\n"
