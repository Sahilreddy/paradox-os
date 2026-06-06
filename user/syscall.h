// int 0x80 stubs for ring-3 programs.

#ifndef PARADOX_USER_SYSCALL_H
#define PARADOX_USER_SYSCALL_H

#define SYS_READ   0
#define SYS_WRITE  1
#define SYS_EXIT   4
#define SYS_HELLO  10

#define STDIN_FD   0
#define STDOUT_FD  1
#define STDERR_FD  2

static inline long syscall0(long n) {
    long r;
    __asm__ volatile ("int $0x80" : "=a"(r) : "a"(n) : "memory");
    return r;
}

static inline long syscall1(long n, long a) {
    long r;
    __asm__ volatile ("int $0x80" : "=a"(r) : "a"(n), "D"(a) : "memory");
    return r;
}

static inline long syscall3(long n, long a, long b, long c) {
    long r;
    __asm__ volatile ("int $0x80"
                      : "=a"(r)
                      : "a"(n), "D"(a), "S"(b), "d"(c)
                      : "memory");
    return r;
}

static inline unsigned long ustrlen(const char* s) {
    unsigned long n = 0;
    while (s[n]) n++;
    return n;
}

static inline long uwrite(int fd, const char* s, unsigned long n) {
    return syscall3(SYS_WRITE, fd, (long)s, (long)n);
}

static inline long uwrites(const char* s) {
    return uwrite(STDOUT_FD, s, ustrlen(s));
}

static inline long uread(int fd, char* buf, unsigned long n) {
    return syscall3(SYS_READ, fd, (long)buf, (long)n);
}

static inline void uexit(int code) {
    syscall1(SYS_EXIT, code);
}

#endif
