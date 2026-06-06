#include "syscall.h"

int main(int argc, char** argv) {
    for (int i = 1; i < argc; i++) {
        if (i > 1) uwrites(" ");
        uwrites(argv[i]);
    }
    uwrites("\n");
    return 0;
}
