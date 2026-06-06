#include "syscall.h"

int main(int argc, char** argv) {
    syscall1(SYS_HELLO, 0);

    uwrites("And this line was written by sys_write() from a real ELF binary.\n"
            "Compiled separately, loaded by the kernel ELF loader, running\n"
            "in CPL=3 with its own user stack. Welcome to userspace!\n");

    uwrites("argv: ");
    for (int i = 0; i < argc; i++) {
        if (i > 0) uwrites(" ");
        uwrites(argv[i]);
    }
    uwrites("\n");

    return 0;
}
