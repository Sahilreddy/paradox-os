# ParadoxOS Makefile

# Architecture
ARCH := x86_64

# Compiler and tools
ASM := nasm
# Use native compiler for now (works on x86_64 Linux building for x86_64 kernel)
# For production, use: CC := x86_64-elf-gcc, CXX := x86_64-elf-g++
CC := gcc
CXX := g++
LD := ld
QEMU := qemu-system-$(ARCH)

# Directories
SRC_DIR := src
BUILD_DIR := build
BOOT_DIR := $(SRC_DIR)/boot
KERNEL_DIR := $(SRC_DIR)/kernel
ISO_DIR := $(BUILD_DIR)/iso

# Flags
ASMFLAGS := -f elf64
CFLAGS := -ffreestanding -O2 -Wall -Wextra -fno-exceptions -fno-rtti \
          -mno-red-zone -mno-mmx -mno-sse -mno-sse2 -mcmodel=large \
          -nostdlib -I$(SRC_DIR)/include
CXXFLAGS := $(CFLAGS) -std=c++17 -fno-use-cxa-atexit
LDFLAGS := -T $(BOOT_DIR)/linker.ld -nostdlib -z max-page-size=0x1000

# Files
BOOT_ASM := $(wildcard $(BOOT_DIR)/*.asm)
KERNEL_C := $(wildcard $(KERNEL_DIR)/*.c)
KERNEL_CPP := $(wildcard $(KERNEL_DIR)/*.cpp)
KERNEL_ASM := $(wildcard $(KERNEL_DIR)/*.asm)

BOOT_OBJ := $(patsubst $(BOOT_DIR)/%.asm,$(BUILD_DIR)/boot/%.o,$(BOOT_ASM))
KERNEL_C_OBJ := $(patsubst $(KERNEL_DIR)/%.c,$(BUILD_DIR)/kernel/%.o,$(KERNEL_C))
KERNEL_CPP_OBJ := $(patsubst $(KERNEL_DIR)/%.cpp,$(BUILD_DIR)/kernel/%.o,$(KERNEL_CPP))
KERNEL_ASM_OBJ := $(patsubst $(KERNEL_DIR)/%.asm,$(BUILD_DIR)/kernel/%.o,$(KERNEL_ASM))

ALL_OBJ := $(BOOT_OBJ) $(KERNEL_C_OBJ) $(KERNEL_CPP_OBJ) $(KERNEL_ASM_OBJ)

# Targets
KERNEL_BIN := $(BUILD_DIR)/paradoxos.bin
ISO := $(BUILD_DIR)/paradoxos.iso

.PHONY: all clean run debug iso

all: $(KERNEL_BIN)

# Link kernel
$(KERNEL_BIN): $(ALL_OBJ)
	@mkdir -p $(BUILD_DIR)
	$(LD) $(LDFLAGS) -o $@ $^
	@echo "Built kernel: $(KERNEL_BIN)"

# Compile boot assembly
$(BUILD_DIR)/boot/%.o: $(BOOT_DIR)/%.asm
	@mkdir -p $(BUILD_DIR)/boot
	$(ASM) $(ASMFLAGS) $< -o $@

# Compile kernel assembly
$(BUILD_DIR)/kernel/%.o: $(KERNEL_DIR)/%.asm
	@mkdir -p $(BUILD_DIR)/kernel
	$(ASM) $(ASMFLAGS) $< -o $@

# Compile kernel C
$(BUILD_DIR)/kernel/%.o: $(KERNEL_DIR)/%.c
	@mkdir -p $(BUILD_DIR)/kernel
	$(CC) $(CFLAGS) -c $< -o $@

# Compile kernel C++
$(BUILD_DIR)/kernel/%.o: $(KERNEL_DIR)/%.cpp
	@mkdir -p $(BUILD_DIR)/kernel
	$(CXX) $(CXXFLAGS) -c $< -o $@

# Create ISO
iso: $(KERNEL_BIN)
	@mkdir -p $(ISO_DIR)/boot/grub
	@cp $(KERNEL_BIN) $(ISO_DIR)/boot/
	@echo 'set timeout=3' > $(ISO_DIR)/boot/grub/grub.cfg
	@echo 'set default=0' >> $(ISO_DIR)/boot/grub/grub.cfg
	@echo '' >> $(ISO_DIR)/boot/grub/grub.cfg
	@echo 'menuentry "ParadoxOS" {' >> $(ISO_DIR)/boot/grub/grub.cfg
	@echo '    multiboot2 /boot/paradoxos.bin' >> $(ISO_DIR)/boot/grub/grub.cfg
	@echo '    boot' >> $(ISO_DIR)/boot/grub/grub.cfg
	@echo '}' >> $(ISO_DIR)/boot/grub/grub.cfg
	@grub-mkrescue -o $(ISO) $(ISO_DIR) 2>&1 | grep -v "xorriso"
	@echo "Created ISO: $(ISO)"

# Run in QEMU
run: iso
	$(QEMU) -cdrom $(ISO) -m 512M -serial stdio

# Debug in QEMU with GDB
debug: iso
	$(QEMU) -cdrom $(ISO) -m 512M -serial stdio -s -S

# Clean
clean:
	rm -rf $(BUILD_DIR)

# Install cross-compiler (helper)
install-tools:
	@echo "Installing development tools..."
	@echo "This will install: nasm, qemu-system-x86, grub-pc-bin, xorriso, mtools"
	sudo apt update
	sudo apt install -y nasm qemu-system-x86 grub-pc-bin xorriso mtools build-essential
	@echo "For cross-compiler, follow: https://wiki.osdev.org/GCC_Cross-Compiler"
