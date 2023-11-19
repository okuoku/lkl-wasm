#ifndef YUNI_W2C_FIXUP_WASMLINUX_USER
#define YUNI_W2C_FIXUP_WASMLINUX_USER

#ifdef __cplusplus
extern "C" {
#endif
// }

#include <setjmp.h>

int wasmlinux_run_to_execve(jmp_buf* jb);

#define w2c_wasmlinux__hooks_vfork(x) \
    ({int r; jmp_buf jb; r = setjmp(jb); \
     if(!r) {wasmlinux_run_to_execve(&jb);} \
     r;})

#endif

#ifdef __cplusplus
}
#endif
