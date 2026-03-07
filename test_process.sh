#!/bin/bash
# Test ParadoxOS with process management

cd "$(dirname "$0")"

echo "=========================================="
echo "  Testing ParadoxOS Phase 3"
echo "=========================================="

# Build
make clean > /dev/null 2>&1
make > /dev/null 2>&1
make iso > /dev/null 2>&1

if [ $? -eq 0 ]; then
    echo "✓ Build successful"
    
    # Run with serial output
    echo ""
    echo "=========================================="
    echo "  Boot Output:"
    echo "=========================================="
    
    # Start QEMU with serial output to file
    timeout 3s qemu-system-x86_64 \
        -cdrom build/paradoxos.iso \
        -m 512M \
        -serial file:test_serial.log \
        -display none \
        > /dev/null 2>&1
    
    # Show output
    if [ -f test_serial.log ]; then
        cat test_serial.log
        echo ""
        echo "=========================================="
        echo "  Test Complete!"
        echo "=========================================="
    else
        echo "ERROR: No serial output captured"
    fi
else
    echo "✗ Build failed"
    exit 1
fi
