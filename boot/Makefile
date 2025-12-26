BOOT_DIR := boot
BUILD_DIR := build/boot
ESP_DIR := $(BUILD_DIR)/esp_root
KERNEL := $(BUILD_DIR)/kernel.elf
ESP := $(BUILD_DIR)/rse_esp.img
LIMINE_DIR := $(BOOT_DIR)/limine
CC := gcc

CFLAGS := -Wall -Wextra -O2 -ffreestanding -fno-stack-protector -fno-pic -fno-pie \
	-mno-red-zone -mcmodel=kernel -nostdlib -nostartfiles -fno-builtin -std=c11
LDFLAGS := -T $(BOOT_DIR)/linker.ld -nostdlib -Wl,-z,max-page-size=0x1000 -no-pie

.PHONY: all iso run clean limine

all: $(ESP)

$(BUILD_DIR):
	mkdir -p $(BUILD_DIR)

$(KERNEL): $(BOOT_DIR)/kernel.c $(BOOT_DIR)/linker.ld | $(BUILD_DIR)
	$(CC) $(CFLAGS) -c $(BOOT_DIR)/kernel.c -o $(BUILD_DIR)/kernel.o
	$(CC) $(CFLAGS) $(LDFLAGS) -o $(KERNEL) $(BUILD_DIR)/kernel.o

limine: $(LIMINE_DIR)/limine-uefi-cd.bin $(LIMINE_DIR)/limine-uefi.bin

$(LIMINE_DIR)/limine-uefi-cd.bin $(LIMINE_DIR)/limine-uefi.bin:
	bash $(BOOT_DIR)/fetch_limine.sh

$(ESP): $(KERNEL) $(BOOT_DIR)/limine.conf | $(BUILD_DIR) limine
	rm -rf $(ESP_DIR)
	mkdir -p $(ESP_DIR)/EFI/BOOT
	cp $(KERNEL) $(ESP_DIR)/kernel.elf
	cp $(BOOT_DIR)/limine.conf $(ESP_DIR)/limine.conf
	cp $(BOOT_DIR)/limine.conf $(ESP_DIR)/EFI/BOOT/limine.conf
	cp $(LIMINE_DIR)/limine-uefi.bin $(ESP_DIR)/EFI/BOOT/BOOTX64.EFI
	cp $(KERNEL) $(ESP_DIR)/EFI/BOOT/kernel.elf
	dd if=/dev/zero of=$(ESP) bs=1M count=64
	mformat -i $(ESP) -F ::
	mcopy -D o -s -i $(ESP) $(ESP_DIR)/* ::

iso: $(ESP)

run: $(ESP)
	@if [ ! -f $(BUILD_DIR)/OVMF_VARS.fd ]; then \
		cp /usr/share/OVMF/OVMF_VARS_4M.fd $(BUILD_DIR)/OVMF_VARS.fd; \
	fi
	qemu-system-x86_64 -machine q35 -m 512M \
		-drive if=pflash,format=raw,readonly=on,file=/usr/share/OVMF/OVMF_CODE_4M.fd \
		-drive if=pflash,format=raw,file=$(BUILD_DIR)/OVMF_VARS.fd \
		-drive if=virtio,format=raw,file=$(ESP) -serial stdio -display sdl

clean:
	rm -rf $(BUILD_DIR)
