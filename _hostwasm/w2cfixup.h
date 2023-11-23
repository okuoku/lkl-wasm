#ifndef YUNI_W2C_FIXUP_WASMLINUX_USER
#define YUNI_W2C_FIXUP_WASMLINUX_USER

#ifdef __cplusplus
extern "C" {
#endif
// }

#include <string.h>
#include <setjmp.h>

int wasmlinux_run_to_execve(jmp_buf* jb);

// FIXME: implement sigsetjmp/siglongjmp
// FIXME: Consider putting setjmp inside if, if(setjmp(...))

#define w2c_wasmlinux__hooks_vfork(x) \
    ({int r; jmp_buf jb; r = setjmp(jb); \
     if(!r) {wasmlinux_run_to_execve(&jb);} \
     r;})

#define w2c_wasmlinux__hooks__setjmp w2c_wasmlinux__hooks_setjmp
#define w2c_wasmlinux__hooks_sigsetjmp(n,x,y) w2c_wasmlinux__hooks_setjmp(n,x)

#define w2c_wasmlinux__hooks_setjmp(n,x) \
    ({int r; jmp_buf jb; void* p; \
     p = &jb; memcpy(&x, &p, sizeof(void*)); \
     r = setjmp(jb); \
     r;})

#define w2c_wasmlinux__hooks_siglongjmp w2c_wasmlinux__hooks_longjmp
#define w2c_wasmlinux__hooks__longjmp w2c_wasmlinux__hooks_longjmp

#define w2c_wasmlinux__hooks_longjmp(n,x,y) \
    ({int r; jmp_buf* p; \
     memcpy(&p, &x, sizeof(void*)); \
     longjmp(*p, y);})


#endif

#ifdef __cplusplus
}
#endif
