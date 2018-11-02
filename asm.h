/* -*- mode: asm; -*- */
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

#ifndef __ASM_H__
#define __ASM_H__

#if defined(__x86_64__)
.macro pusha
.irp reg rax,rdi,rsi,rdx,rcx,r8,r9,r10,r11
  push %\reg
.endr
.endm

.macro popa
.irp reg r11,r10,r9,r8,rcx,rdx,rsi,rdi,rax
  pop %\reg
.endr
.endm

/* x86_64 function parameter:	rdi,rsi,rdx,rcx,r8,r9 */

.macro call_function1 function arg0
  mov \arg0, %rdi
  callq \function
.endm

.macro call_function2 function arg0 arg1
  mov \arg0, %rdi
  mov \arg1, %rsi
  callq \function
.endm

#else /* !defined(__x86_64__) */
.macro call_function1 function arg0
  push \arg0
  call \function
.endm

.macro call_function2 function arg0 arg1
  push \arg1
  push \arg0
  call \function
.endm
#endif

#endif /* __ASM_H__ */
