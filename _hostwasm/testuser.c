/* Import syscall I/F */
#include <stdint.h>
__attribute__((import_module("env"), import_name("wasmlinux_syscall32")))
int32_t wasmlinux_syscall32(int argc, uint32_t no, uint32_t* in);

int main(int, char**);
/* Test global variables */
static void* __attribute__((used)) mainptr = (void*)main;
static void** __attribute__((used)) mainptrptr = &mainptr;

int
main(int ac, char** av){
    const char msg[] = "hello, world!\n";
    uint32_t args[6];
    args[0] = (uint32_t)msg;
    args[1] = sizeof(msg);
    (void)wasmlinux_syscall32(2, 64 /* __NR_write */, args);
    return (intptr_t)mainptr & (intptr_t)mainptrptr;
}

