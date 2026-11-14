CC = gcc
AS = nasm
LD = ld

CFLAGS = -m32 -ffreestanding -fno-builtin -nostdlib -O0 -Wall -Wextra -Isrc
ASFLAGS = -f elf32
LDFLAGS = -m elf_i386 -T linker.ld -nostdlib

OBJS = boot.o kernel.o vga.o ps2.o heap.o fs.o speaker.o c_compiler.o cu_runner.o gui.o

.PHONY: all run clean

all: ibrahim_os.iso

boot.o: src/boot.asm
	$(AS) $(ASFLAGS) $< -o $@

%.o: src/%.c src/os.h
	$(CC) $(CFLAGS) -c $< -o $@

ibrahim_os.bin: $(OBJS) linker.ld
	$(LD) $(LDFLAGS) $(OBJS) -o $@

ibrahim_os.iso: ibrahim_os.bin
	mkdir -p isodir/boot/grub
	cp ibrahim_os.bin isodir/boot/ibrahim_os.bin
	@echo 'set timeout=0' > isodir/boot/grub/grub.cfg
	@echo 'set default=0' >> isodir/boot/grub/grub.cfg
	@echo 'menuentry "Ibrahim OS" {' >> isodir/boot/grub/grub.cfg
	@echo '    insmod all_video' >> isodir/boot/grub/grub.cfg
	@echo '    set gfxpayload=640x480x32' >> isodir/boot/grub/grub.cfg
	@echo '    multiboot /boot/ibrahim_os.bin' >> isodir/boot/grub/grub.cfg
	@echo '    boot' >> isodir/boot/grub/grub.cfg
	@echo '}' >> isodir/boot/grub/grub.cfg
	grub-mkrescue -o ibrahim_os.iso isodir

run: ibrahim_os.iso
	qemu-system-i386 -cdrom ibrahim_os.iso -m 256 -vga std -audiodev id=pa,driver=pa -machine pcspk-audiodev=pa || qemu-system-i386 -cdrom ibrahim_os.iso -m 128 -vga std -audiodev id=sdl,driver=sdl -machine pcspk-audiodev=sdl || qemu-system-i386 -cdrom ibrahim_os.iso -m 128 -vga std

clean:
	rm -rf $(OBJS) ibrahim_os.bin ibrahim_os.iso isodir
