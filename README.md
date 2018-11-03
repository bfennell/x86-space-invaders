# x86-space-invaders
A bootable 32-bit/64-bit x86 Space Invaders emulator written in C/Assembler. The code is useful for anyone wanting to learn about the Intel 8080, x86 32-bit protected mode or 64-bit long mode. It runs in QEMU and on real hardware, the Makefile contains rules for building a disk image with GRUB as the boot loader. 

The code executes as a simple loop emulating instructions of the Intel 8080 with interrupts handling keyboard input and graphics update. printf goes to COM1 (port 0x3f8), when running QEMU the "-serial stdio" option sends the printf output to the terminal QEMU was launched from. The 8080 ROM image to execute is loaded as a multiboot1 module, the roms directory contains two images:

* roms/invaders.rom
* roms/cpudiag.rom - an Intel 8080 test suite

The disk images created by the Makefile contain GRUB entries to select which ROM to run.

## To build:
    make all

## To run:
    # run (32-bit) with qemu-system-i386
    make run-i386
    # run (64-bit) with qemu-system-x86_64
    make run-x86_64

# Requirements
No external libraries are required, very basic support is provided in stdio.c/memset.c. Building requires **i686-elf-gcc** for 32-bit and **x86_64-elf-gcc** for 64-bit. If those compilers are not available in your environment then take a look at https://wiki.osdev.org/Bare_bones

# How to play

Computer | Space Invaders
--- | ---
5 | insert a coin
1 | start a game
left | move left
right | move right
space | shoot
ESC | halt the emulator (requires reset to restart)

# Useful Links
* [Intel® 64 and IA-32 Architectures Software Developer Manuals](https://software.intel.com/en-us/articles/intel-sdm)
* [Multiboot](https://www.gnu.org/software/grub/manual/multiboot/multiboot.html)
* [Computer Archeology - Space Invaders](http://computerarcheology.com/Arcade/SpaceInvaders)
* [OSDev.org](https://wiki.osdev.org/Main_Page)
* [cpudiag.asm](http://stateoftheark.ca:8080/Pages/Files/WC_CPM/simtel/sigm/vols000/vol005/cpudiag.asm)
