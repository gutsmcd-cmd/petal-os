CFLAGS=-m32 -ffreestanding -fno-pie -fno-stack-protector -nostdlib -O2 -Wall -Wno-unused-variable -fno-builtin -march=i686
all: petal-os.iso
src/font.h src/files.h: gen.py src/kernel.c $(wildcard files/*.txt files/*.html)
	python3 gen.py
build/kernel.o: src/kernel.c src/font.h src/files.h
	mkdir -p build && gcc $(CFLAGS) -c $< -o $@
build/boot.o: src/boot.asm
	mkdir -p build && nasm -f elf32 $< -o $@
iso/boot/petal.elf: build/boot.o build/kernel.o src/linker.ld
	gcc -m32 -nostdlib -no-pie -T src/linker.ld -o $@ build/boot.o build/kernel.o -lgcc
petal-os.iso: iso/boot/petal.elf iso/boot/grub/grub.cfg
	grub-mkrescue -o $@ iso 2>/dev/null
run: petal-os.iso
	qemu-system-i386 -cdrom petal-os.iso -m 256 -audiodev pa,id=snd -machine pcspk-audiodev=snd -device sb16,audiodev=snd
clean:
	rm -rf build iso/boot/petal.elf petal-os.iso src/font.h src/files.h
