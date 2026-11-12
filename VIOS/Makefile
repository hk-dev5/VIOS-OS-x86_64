# VIOS Makefile - freestanding x86-64 kernel + Limine ISO

ARCH        := x86_64
TARGET      := $(ARCH)-elf
CC          := gcc
LD          := ld
AS          := nasm

CFLAGS      := -std=gnu11 -ffreestanding -fno-stack-protector -fno-stack-check \
               -fno-lto -fno-PIC -m64 -march=x86-64 -mabi=sysv \
               -mno-80387 -mno-mmx -mno-sse -mno-sse2 -mno-red-zone \
               -mcmodel=kernel -Wall -Wextra -O2 \
               -Iinclude -Ikernel/include

LDFLAGS     := -nostdlib -static -T linker/linker.ld -z max-page-size=0x1000

KERNEL_SRC  := kernel/src/console.c kernel/src/kernel.c
KERNEL_OBJ  := $(KERNEL_SRC:.c=.o)
KERNEL_ELF  := build/vios.elf
ISO         := build/VIOS.iso

LIMINE_DIR  := ../limine
LIMINE_BIN  := $(LIMINE_DIR)

.PHONY: all clean iso run test dirs

all: iso

dirs:
	mkdir -p build iso_root/EFI/BOOT iso_root/boot

$(KERNEL_OBJ): %.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

$(KERNEL_ELF): dirs $(KERNEL_OBJ)
	$(LD) $(LDFLAGS) $(KERNEL_OBJ) -o $@
	@echo "Kernel built: $@"

iso: $(KERNEL_ELF)
	@echo "Assembling ISO..."
	rm -rf iso_root
	mkdir -p iso_root/boot iso_root/EFI/BOOT
	cp $(KERNEL_ELF) iso_root/boot/vios.elf
	cp boot/limine.conf iso_root/limine.conf
	# Limine files
	cp $(LIMINE_BIN)/limine-bios.sys iso_root/ 2>/dev/null || true
	cp $(LIMINE_BIN)/limine-bios-cd.bin iso_root/ 2>/dev/null || true
	cp $(LIMINE_BIN)/limine-uefi-cd.bin iso_root/ 2>/dev/null || true
	cp $(LIMINE_BIN)/BOOTX64.EFI iso_root/EFI/BOOT/ 2>/dev/null || true
	# Create ISO (hybrid BIOS + UEFI)
	xorriso -as mkisofs -R -r -J -b limine-bios-cd.bin \
		-no-emul-boot -boot-load-size 4 -boot-info-table \
		--efi-boot limine-uefi-cd.bin \
		-efi-boot-part --efi-boot-image --protective-msdos-label \
		iso_root -o $(ISO)
	# Install Limine BIOS bootloader into the ISO image
	$(LIMINE_BIN)/limine bios-install $(ISO) 2>/dev/null || \
		../limine/limine bios-install $(ISO) 2>/dev/null || \
		echo "Note: limine bios-install skipped (tool may need build)"
	@echo ""
	@echo "=== ISO ready: $(ISO) ==="
	@ls -lh $(ISO)

# Build limine host tool if needed
limine-tool:
	cd $(LIMINE_DIR) && make 2>/dev/null || true

run: iso
	qemu-system-x86_64 -m 256M -cdrom $(ISO) -serial stdio -no-reboot -no-shutdown

test: iso
	timeout 25 qemu-system-x86_64 -m 256M -cdrom $(ISO) -serial stdio -display none -no-reboot 2>&1 || true

clean:
	rm -f kernel/src/*.o
	rm -rf build iso_root
	rm -f $(ISO)

.PHONY: help
help:
	@echo "VIOS build targets:"
	@echo "  make          - build ISO"
	@echo "  make run      - build and run in QEMU (GUI)"
	@echo "  make test     - build and quick headless boot test"
	@echo "  make clean    - remove build artifacts"
