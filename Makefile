UNAME_M := $(shell uname -m)
ifeq ($(UNAME_M),aarch64)
PREFIX:=i386-unknown-elf-
BOOTIMG:=/usr/local/grub/lib/grub/i386-pc/boot.img
GRUBLOC:=/usr/local/grub/bin/
else
PREFIX:=
BOOTIMG:=/usr/lib/grub/i386-pc/boot.img
GRUBLOC :=
endif
CC := $(PREFIX)gcc
LD := $(PREFIX)ld
OBJDUMP := $(PREFIX)objdump
OBJCOPY := $(PREFIX)objcopy
SIZE := $(PREFIX)size
CONFIGS := -DCONFIG_HEAP_SIZE=4096
CFLAGS := -ffreestanding -mgeneral-regs-only -mno-mmx -m32 -march=i386 -fno-pie -fno-stack-protector -g3 -Wall 
ODIR = obj
SDIR = src
OBJS = \
	kernel_main.o \
	rprintf.o \
	interrupt.o \
	page.o \
	sd.o \
	fat.o 
# Make sure to keep a blank line here after OBJS list

OBJ = $(patsubst %,$(ODIR)/%,$(OBJS))

$(ODIR)/fat.o: fat.c
	$(CC) $(CFLAGS) -c -g -o $@ $^

$(ODIR)/%.o: $(SDIR)/%.c
	$(CC) $(CFLAGS) -c -g -o $@ $^
	
$(ODIR)/%.o: $(SDIR)/%.s
	$(CC) $(CFLAGS) -c -g -o $@ $^

$(ODIR)/page.o: page.c
	$(CC) $(CFLAGS) -c -g -o $@ $^

$(ODIR)/sd.o: sd.c
	$(CC) $(CFLAGS) -c -g -o $@ $^

$(ODIR)/rprintf.o: rprintf.c
	$(CC) $(CFLAGS) -c -g -o $@ $^

$(ODIR)/interrupt.o: interrupt.c
	$(CC) $(CFLAGS) -c -g -o $@ $^
	
$(ODIR)/page.o: page.c
	$(CC) $(CFLAGS) -c -g -o $@ $^

all: bin rootfs.img

bin: obj $(OBJ)
	$(LD) -melf_i386  obj/* -Tkernel.ld -o kernel
	$(SIZE) kernel

obj:
	mkdir -p obj

rootfs.img:
	dd if=/dev/zero of=rootfs.img bs=1M count=32
	$(GRUBLOC)grub-mkimage -p "(hd0,msdos1)/boot" -o grub.img -O i386-pc normal biosdisk multiboot multiboot2 configfile fat exfat part_msdos
	dd if=$(BOOTIMG) of=rootfs.img conv=notrunc
	dd if=grub.img of=rootfs.img conv=notrunc bs=512 seek=1
	echo 'start=2048, type=83, bootable' | sfdisk rootfs.img
	mkfs.vfat --offset 2048 -F16 rootfs.img
	mcopy -i rootfs.img@@1M kernel ::/
	mmd -i rootfs.img@@1M boot 
	mcopy -i rootfs.img@@1M grub.cfg ::/boot
	@echo " -- BUILD COMPLETED SUCCESSFULLY --"
disk.img:
	dd if=/dev/zero of=disk.img bs=1M count=32
	mformat -F -t 64 -h 16 -s 32 -i disk.img ::
	mcopy -i disk.img kernel ::
	echo "Hello from FAT filesystem!" > test_temp.txt
	mcopy -i disk.img test_temp.txt ::TEST.TXT
	rm -f test_temp.txt
	mmd -i disk.img boot
	mcopy -i disk.img grub.cfg ::boot/grub.cfg
	$(GRUBLOC)grub-mkimage -p /boot -o grub.img -O i386-pc normal biosdisk multiboot multiboot2 configfile fat
	dd if=$(BOOTIMG) of=disk.img conv=notrunc
	dd if=grub.img of=disk.img conv=notrunc bs=512 seek=1
	@echo " -- DISK.IMG BUILD COMPLETED --"

rundisk: bin disk.img
	qemu-system-x86_64 -hda disk.img

run:
	qemu-system-x86_64 -hda rootfs.img

debug:
	./launch_qemu.sh
	screen -S qemu -d -m qemu-system-i386 -S -s -hda rootfs.img -monitor stdio
	TERM=xterm i386-unkown-elf-gdb -x gdb_os.txt && killall qemu-system-i386

clean:
	rm -f grub.img kernel rootfs.img obj/*

