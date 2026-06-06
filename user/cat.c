#include "syscall.h"

int main(int argc, char** argv) {
    (void)argc; (void)argv;

    uwrites("[cat] type a line and press Enter:\n");

    char buf[128];
    long n = uread(STDIN_FD, buf, sizeof(buf));
    if (n < 0) {
        uwrites("[cat] read failed\n");
        return 1;
    }
    uwrites("[cat] you typed: ");
    uwrite(STDOUT_FD, buf, (unsigned long)n);
    if (n == 0 || buf[n - 1] != '\n') uwrites("\n");
    return 0;
}
