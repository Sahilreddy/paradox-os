# ParadoxOS - Custom Operating System

A custom operating system built from scratch with AI capabilities.

## Hardware Requirements
- CPU: Intel i7-14650HX (24 threads)
- GPU: NVIDIA GeForce RTX 4060 Laptop
- Host: Ubuntu 24.04.3 LTS

## Features (Planned)
- Custom bootloader
- x86_64 kernel written in C++ and Assembly
- Memory management (paging, virtual memory)
- Process and thread management
- Custom filesystem
- Display drivers
- NVIDIA GPU integration
- AI inference engine with GPU acceleration
- Custom shell and utilities

## Build Instructions
```bash
make          # Build the OS
make run      # Run in QEMU
make debug    # Run with GDB
make clean    # Clean build artifacts
```

## Development Phases
1. **Phase 1**: Bootloader + Basic Kernel
2. **Phase 2**: Memory Management + Interrupts
3. **Phase 3**: Process Management
4. **Phase 4**: Drivers (Display, Input, Storage)
5. **Phase 5**: Filesystem
6. **Phase 6**: GPU Integration
7. **Phase 7**: AI Runtime

## Architecture
- **Kernel Type**: Microkernel/Hybrid
- **Target**: x86_64
- **Languages**: C++, Assembly (NASM)
- **Bootloader**: GRUB2 (initially), custom later
