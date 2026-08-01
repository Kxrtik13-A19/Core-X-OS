CC = gcc
CFLAGS = -m32 -ffreestanding -O2 -nostdlib -fno-pie -fno-stack-protector -Wall
AS = nasm
ASFLAGS = -f elf32
LDFLAGS = -m elf_i386 -T linker.ld -nostdlib -z noexecstack

all: corex.iso

boot.o: boot.asm
	$(AS) $(ASFLAGS) boot.asm -o boot.o

kernel.o: kernel.c
	$(CC) $(CFLAGS) -c kernel.c -o kernel.o

corex.bin: boot.o kernel.o
	ld $(LDFLAGS) boot.o kernel.o -o corex.bin

corex.iso: corex.bin
	mkdir -p isodir/boot/grub
	cp corex.bin isodir/boot/corex.bin
	echo 'set timeout=0' > isodir/boot/grub/grub.cfg
	echo 'set default=0' >> isodir/boot/grub/grub.cfg
	echo 'insmod all_video' >> isodir/boot/grub/grub.cfg
	echo 'menuentry "COREX OS" {' >> isodir/boot/grub/grub.cfg
	echo '  set gfxpayload=800x600x32,1024x768x32,auto' >> isodir/boot/grub/grub.cfg
	echo '  multiboot /boot/corex.bin' >> isodir/boot/grub/grub.cfg
	echo '  boot' >> isodir/boot/grub/grub.cfg
	echo '}' >> isodir/boot/grub/grub.cfg
	grub-mkrescue -o corex.iso isodir

run: corex.iso
	qemu-system-i386 -cdrom corex.iso -m 256M -vga std

clean:
	rm -rf *.o corex.bin isodir corex.iso