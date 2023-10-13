#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include "lin.h"

#include "lin.h"

w2c_lin the_linux;

void
w2c_env_nccc_call64(struct w2c_env* env, u32 inptr, u32 outptr){
    uint8_t* inp;
    uint8_t* outp;
    uint64_t* in;
    uint64_t* out;
    uint64_t mod, func;
    wasm_rt_memory_t* mem;
    mem = w2c_lin_memory(&the_linux);

    inp = mem->data + inptr;
    outp = mem->data + outptr;
    in = (uint64_t*)inp;
    out = (uint64_t*)out;

    mod = in[0];
    func = in[1];

    printf("CALL: %ld %ld \n", mod, func);

    switch(mod){
        case 0: /* Admin */
        case 1: /* syncobjects */
        case 2: /* threads */
        case 3: /* memory mgr */
        case 4: /* timer */
        case 5: /* context */
        default:
            printf("Unknown request.\n");
            exit(-1);
            return;
    }
}

int
main(int ac, char** av){
    wasm_rt_init();
    wasm2c_lin_instantiate(&the_linux, 0);
    
    w2c_lin_init(&the_linux);
    return 0;
}
