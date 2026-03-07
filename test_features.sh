#!/bin/bash
echo "=== ParadoxOS Feature Test ==="
cd /home/paradox/Documents/OS
make clean > /dev/null 2>&1 && make > /dev/null 2>&1 && make iso > /dev/null 2>&1
echo "✓ Build complete"
timeout 10 bash -c "qemu-system-x86_64 -cdrom build/paradoxos.iso -m 512M -serial file:/tmp/test.log -display none & sleep 8"
echo ""
echo "Boot Log:"
grep -E "(initialized|Initialized|Timer:|Syscall:|TSS:)" /tmp/test.log 2>/dev/null | head -20
