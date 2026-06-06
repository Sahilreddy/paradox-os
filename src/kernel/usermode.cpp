#include "../include/usermode.h"
#include "../include/memory.h"

extern "C" {
    void usermode_jump(uint64_t entry_rip, uint64_t user_rsp);
    extern uint8_t g_usermode_active;
}

bool usermode_active() { return g_usermode_active != 0; }
const char* usermode_status() { return usermode_active() ? "ring 3" : "kernel"; }

void usermode_run(uint64_t entry_rip, uint64_t user_rsp) {
    usermode_jump(entry_rip, user_rsp);
}

enum { ARGMAX = 16, ARGBUF_CAP = 256 };

static int s_len(const char* s) { int n = 0; while (s[n]) n++; return n; }

static int tokenize(const char* args,
                    const char* tokens[ARGMAX],
                    char scratch[ARGBUF_CAP],
                    int starting_argc) {
    int argc = starting_argc;
    int abi  = 0;
    if (!args) return argc;
    const char* p = args;
    while (*p && argc < ARGMAX) {
        while (*p == ' ' || *p == '\t') p++;
        if (!*p) break;
        tokens[argc] = &scratch[abi];
        while (*p && *p != ' ' && *p != '\t' && abi + 1 < ARGBUF_CAP) {
            scratch[abi++] = *p++;
        }
        if (abi < ARGBUF_CAP) scratch[abi++] = 0;
        argc++;
    }
    return argc;
}

// Builds the SysV process-init frame on top of a fresh user-stack page:
//   [rsp]    argc
//   [rsp+8]  argv[0..argc-1], NULL, envp[0]=NULL
//   [top]    packed string data
void usermode_run_with_args(uint64_t entry_rip,
                            const char* prog_name,
                            const char* args) {
    uint8_t* stack = (uint8_t*)pmm_alloc_page();
    if (!stack) return;
    for (int i = 0; i < 4096; i++) stack[i] = 0xCC;

    const char* tokens[ARGMAX];
    char        scratch[ARGBUF_CAP];
    tokens[0] = prog_name ? prog_name : "";
    int argc  = tokenize(args, tokens, scratch, 1);

    char* sp = (char*)stack + 4096;

    uint64_t arg_ptrs[ARGMAX];
    for (int i = argc - 1; i >= 0; i--) {
        int len = s_len(tokens[i]);
        sp -= len + 1;
        for (int j = 0; j < len; j++) sp[j] = tokens[i][j];
        sp[len] = 0;
        arg_ptrs[i] = (uint64_t)sp;
    }

    sp = (char*)((uintptr_t)sp & ~(uintptr_t)15);

    sp -= 8; *(uint64_t*)sp = 0;        // envp[0]
    sp -= 8; *(uint64_t*)sp = 0;        // argv[argc]
    for (int i = argc - 1; i >= 0; i--) {
        sp -= 8;
        *(uint64_t*)sp = arg_ptrs[i];
    }
    sp -= 8; *(uint64_t*)sp = (uint64_t)argc;

    usermode_run(entry_rip, (uint64_t)sp);

    pmm_free_page(stack);
}
