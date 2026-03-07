#!/bin/bash
# ParadoxOS Development Environment Setup

set -e

echo "======================================"
echo "  ParadoxOS Development Setup"
echo "======================================"
echo ""

# Check OS
if [ -f /etc/os-release ]; then
    . /etc/os-release
    echo "Detected OS: $NAME $VERSION"
else
    echo "Cannot detect OS. This script is for Linux systems."
    exit 1
fi

# Update package list
echo ""
echo "Updating package list..."
sudo apt update

# Install basic tools
echo ""
echo "Installing basic development tools..."
sudo apt install -y build-essential nasm qemu-system-x86 grub-pc-bin xorriso mtools git

# Check for cross-compiler
echo ""
echo "Checking for x86_64-elf cross-compiler..."
if ! command -v x86_64-elf-gcc &> /dev/null; then
    echo ""
    echo "WARNING: x86_64-elf-gcc not found!"
    echo "You need to build a cross-compiler."
    echo ""
    echo "Options:"
    echo "  1. Follow the guide: https://wiki.osdev.org/GCC_Cross-Compiler"
    echo "  2. Run: ./scripts/build-cross-compiler.sh (if available)"
    echo ""
    read -p "Do you want to see the quick setup instructions? (y/n) " -n 1 -r
    echo
    if [[ $REPLY =~ ^[Yy]$ ]]; then
        echo ""
        echo "Quick Cross-Compiler Setup:"
        echo "======================================"
        echo "export PREFIX=\"\$HOME/opt/cross\""
        echo "export TARGET=x86_64-elf"
        echo "export PATH=\"\$PREFIX/bin:\$PATH\""
        echo ""
        echo "# Install dependencies"
        echo "sudo apt install -y build-essential bison flex libgmp3-dev libmpc-dev libmpfr-dev texinfo"
        echo ""
        echo "# Download binutils and gcc"
        echo "mkdir -p ~/src"
        echo "cd ~/src"
        echo "wget https://ftp.gnu.org/gnu/binutils/binutils-2.40.tar.gz"
        echo "wget https://ftp.gnu.org/gnu/gcc/gcc-13.2.0/gcc-13.2.0.tar.gz"
        echo ""
        echo "# Build binutils"
        echo "tar -xzf binutils-2.40.tar.gz"
        echo "mkdir build-binutils && cd build-binutils"
        echo "../binutils-2.40/configure --target=\$TARGET --prefix=\"\$PREFIX\" --with-sysroot --disable-nls --disable-werror"
        echo "make -j\$(nproc)"
        echo "make install"
        echo ""
        echo "# Build gcc"
        echo "cd ~/src"
        echo "tar -xzf gcc-13.2.0.tar.gz"
        echo "mkdir build-gcc && cd build-gcc"
        echo "../gcc-13.2.0/configure --target=\$TARGET --prefix=\"\$PREFIX\" --disable-nls --enable-languages=c,c++ --without-headers"
        echo "make all-gcc -j\$(nproc)"
        echo "make all-target-libgcc -j\$(nproc)"
        echo "make install-gcc"
        echo "make install-target-libgcc"
        echo ""
        echo "# Add to PATH (add to ~/.bashrc)"
        echo "export PATH=\"\$HOME/opt/cross/bin:\$PATH\""
        echo "======================================"
        echo ""
    fi
else
    echo "x86_64-elf-gcc found at: $(which x86_64-elf-gcc)"
    x86_64-elf-gcc --version
fi

echo ""
echo "======================================"
echo "Setup complete!"
echo ""
echo "Next steps:"
echo "  1. If you don't have the cross-compiler, build it (see above)"
echo "  2. Run 'make' to build ParadoxOS"
echo "  3. Run 'make run' to test in QEMU"
echo "======================================"
