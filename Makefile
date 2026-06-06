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

USER_PROGRAMS := hello echo cat
USER_ELF_OBJS := $(addprefix $(BUILD_DIR)/user_,$(addsuffix .elf.o,$(USER_PROGRAMS)))

ALL_OBJ := $(BOOT_OBJ) $(KERNEL_C_OBJ) $(KERNEL_CPP_OBJ) $(KERNEL_ASM_OBJ) \
           $(USER_ELF_OBJS)

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

# ----- Userland ELF -------------------------------------------------------
# Compile each user/<name>.c into its own ELF, link it with the shared
# crt0.asm (which sets up argc/argv from the user stack), then convert
# the linked ELF into a binary blob the kernel can embed. The kernel
# references the blobs via the objcopy-injected symbols
# `_binary_build_user_<name>_elf_start/_end`.
USER_DIR     := user
USER_LD      := $(USER_DIR)/user.ld
USER_CRT_SRC := $(USER_DIR)/crt0.asm
USER_CRT_OBJ := $(BUILD_DIR)/user_crt0.o

USER_CFLAGS  := -ffreestanding -nostdlib -fno-pic -mno-red-zone \
                -fno-stack-protector -Wall -O1
USER_LDFLAGS := -nostdlib -static -T $(USER_LD)

$(USER_CRT_OBJ): $(USER_CRT_SRC)
	@mkdir -p $(BUILD_DIR)
	$(ASM) $(ASMFLAGS) $< -o $@

# Template: build $(BUILD_DIR)/user_NAME.elf from user/NAME.c + crt0,
# then objcopy it into $(BUILD_DIR)/user_NAME.elf.o.
define USER_PROG_RULE
$$(BUILD_DIR)/user_$(1).o: $$(USER_DIR)/$(1).c $$(USER_DIR)/syscall.h
	@mkdir -p $$(BUILD_DIR)
	$$(CC) $$(USER_CFLAGS) -c $$< -o $$@

$$(BUILD_DIR)/user_$(1).elf: $$(BUILD_DIR)/user_$(1).o $$(USER_CRT_OBJ) $$(USER_LD)
	$$(LD) $$(USER_LDFLAGS) -o $$@ $$(USER_CRT_OBJ) $$(BUILD_DIR)/user_$(1).o

$$(BUILD_DIR)/user_$(1).elf.o: $$(BUILD_DIR)/user_$(1).elf
	objcopy -I binary -O elf64-x86-64 -B i386:x86-64 \
	    --rename-section .data=.user_blob,alloc,load,readonly,data,contents \
	    $$< $$@
endef

$(foreach prog,$(USER_PROGRAMS),$(eval $(call USER_PROG_RULE,$(prog))))

DISK := $(BUILD_DIR)/paradox-disk.img

# A small (4 MiB) disk image with a recognizable header. Lets the ATA
# driver actually find something to read; the contents are arbitrary.
$(DISK):
	@mkdir -p $(BUILD_DIR)
	@dd if=/dev/zero of=$(DISK) bs=1M count=4 2>/dev/null
	@printf 'ParadoxOS sample disk\nbuilt by Makefile\n' \
	  | dd of=$(DISK) conv=notrunc 2>/dev/null

# Run in QEMU
run: iso $(DISK)
	$(QEMU) -cdrom $(ISO) -drive file=$(DISK),format=raw,if=ide,index=0 \
	        -m 512M -serial stdio

# Debug in QEMU with GDB
debug: iso $(DISK)
	$(QEMU) -cdrom $(ISO) -drive file=$(DISK),format=raw,if=ide,index=0 \
	        -m 512M -serial stdio -s -S

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
