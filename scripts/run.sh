#!/bin/bash
# Quick test script for ParadoxOS

cd "$(dirname "$0")/.."

echo "========================================"
echo "  Building ParadoxOS..."
echo "========================================"
make clean > /dev/null 2>&1
make iso 2>&1 | grep -E "(Built|Created)"

if [ $? -eq 0 ]; then
    echo ""
    echo "✓ Build successful!"
    echo "✓ Multiboot2 valid: $(grub-file --is-x86-multiboot2 build/paradoxos.bin && echo YES || echo NO)"
    echo ""
    echo "========================================"
    echo "  Launching ParadoxOS in QEMU"
    echo "========================================"
    echo ""
    echo "Controls:"
    echo "  - Ctrl+Alt+G to release mouse"
    echo "  - Ctrl+Alt+F to fullscreen"
    echo "  - Close window or Ctrl+C to exit"
    echo ""
    echo "Serial output will appear below:"
    echo "----------------------------------------"
    
    qemu-system-x86_64 \
        -cdrom build/paradoxos.iso \
        -m 512M \
        -serial stdio \
        -vga std \
        -display gtk,zoom-to-fit=on
else
    echo "✗ Build failed!"
    exit 1
fi

