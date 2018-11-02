#-------------------------------------------------------------------------------
#  Copyright (c) 2018 Brendan Fennell <bfennell@skynet.ie>
#
#  Permission is hereby granted, free of charge, to any person obtaining a copy
#  of this software and associated documentation files (the "Software"), to deal
#  in the Software without restriction, including without limitation the rights
#  to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
#  copies of the Software, and to permit persons to whom the Software is
#  furnished to do so, subject to the following conditions:
#
#  The above copyright notice and this permission notice shall be included in all
#  copies or substantial portions of the Software.
#
#  THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
#  IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
#  FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
#  AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
#  LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
#  OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
#  SOFTWARE.
#
#-------------------------------------------------------------------------------

CFLAGS=-Wall -Wextra -ggdb3 -O2 -Wno-format -I. #-DTRACE_I8080
LDFLAGS=-nostdlib -z max-page-size=0x1000 -Tlink.ld
LIBS=-lgcc

.DEFAULT: all
.PHONY: all
all: disk-i386.img disk-x86_64.img

SRC=main.c keyboard.c graphics.c bdos.c invaders_io.c i8080.c stdio.c memset.c x86.c irq.S start.S

#-------------------------------------------------------------------------------
# pc-invaders-i386
#-------------------------------------------------------------------------------

pc-invaders-i386: Makefile
pc-invaders-i386: $(SRC)
	i686-elf-gcc $(CFLAGS) $(LDFLAGS) -o $@ $(SRC) $(LIBS)

disk-i386.img: pc-invaders-i386 grub.cfg
	grub-file --is-x86-multiboot pc-invaders-i386
	mkdir -p disk-i386/boot/grub
	cp grub.cfg disk-i386/boot/grub/grub.cfg
	cp pc-invaders-i386 disk-i386/boot/pc-invaders
	cp roms/invaders.rom disk-i386/boot/
	cp roms/cpudiag.rom disk-i386/boot/
	grub-mkrescue -o $@ disk-i386

run-i386: disk-i386.img
	qemu-system-i386 -drive if=ide,file=disk-i386.img,format=raw -m 4g -soundhw pcspk -serial stdio

#-------------------------------------------------------------------------------
# pc-invaders-x86_64
#-------------------------------------------------------------------------------
pc-invaders-x86_64: Makefile
pc-invaders-x86_64: $(SRC)
	x86_64-elf-gcc $(CFLAGS) $(LDFLAGS) -o $@ $(SRC) $(LIBS)

disk-x86_64.img: pc-invaders-x86_64 grub.cfg
	grub-file --is-x86-multiboot pc-invaders-x86_64
	mkdir -p disk-x86_64/boot/grub
	cp grub.cfg disk-x86_64/boot/grub/grub.cfg
	cp pc-invaders-x86_64 disk-x86_64/boot/pc-invaders
	cp roms/invaders.rom disk-x86_64/boot/
	cp roms/cpudiag.rom disk-x86_64/boot/
	grub-mkrescue -o $@ disk-x86_64

run-x86_64: disk-x86_64.img
	qemu-system-x86_64 -drive if=ide,file=disk-x86_64.img,format=raw -m 4g -soundhw pcspk -serial stdio

#-------------------------------------------------------------------------------
# Clean
#-------------------------------------------------------------------------------
.PHONY: clean
clean:
	rm -f pc-invaders-i386 disk-i386.img
	rm -rf disk-i386/
	rm -f pc-invaders-x86_64 disk-x86_64.img
	rm -rf disk-x86_64/
