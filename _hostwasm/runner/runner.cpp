#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include "lin.h"

#include <thread>
#include <mutex>
#include <semaphore>

w2c_lin the_linux;

enum objtype{
    OBJTYPE_DUMMY = 0,
    OBJTYPE_FREE = 1,
    OBJTYPE_SEM = 2,
    OBJTYPE_MUTEX = 3,
    OBJTYPE_RECURSIVE_MUTEX = 4,
    OBJTYPE_THREAD = 5,
};

struct hostobj_s {
    enum objtype type;
    int id;
    union {
        std::counting_semaphore<>* sem;
        std::mutex* mtx;
        std::recursive_mutex* mtx_recursive;
        struct {
            uintptr_t func32;
            uintptr_t arg32;
            uintptr_t ret;
            std::thread* thread;
        } thr;
    } obj;
};

#define MAX_HOSTOBJ 4096
std::mutex objmtx;
struct hostobj_s objtbl[MAX_HOSTOBJ];

static int
newobj(objtype type){
    std::lock_guard<std::mutex> NN(objmtx);
    int i;
    for(i=0;i!=MAX_HOSTOBJ;i++){
        if(objtbl[i].type == OBJTYPE_FREE){
            objtbl[i].type = type;
            return i;
        }
    }
    abort();
}

static void
delobj(int idx){
    objtbl[idx].type = OBJTYPE_FREE;
}

static void
mod_syncobjects(uint64_t* in, uint64_t* out){
    int idx;
    switch(in[0]){
        case 1: /* sem_alloc [1 1 count] => [sem32] */
            idx = newobj(OBJTYPE_SEM);
            objtbl[idx].obj.sem = new std::counting_semaphore(in[1]);
            out[0] = idx;
            break;
        case 2: /* sem_free [1 2 sem32] => [] */
            idx = in[1];
            if(objtbl[idx].type != OBJTYPE_SEM){
                abort();
            }
            delete objtbl[idx].obj.sem;
            delobj(idx);
            break;
        case 3: /* sem_up [1 3 sem32] => [] */
            idx = in[1];
            if(objtbl[idx].type != OBJTYPE_SEM){
                abort();
            }
            objtbl[idx].obj.sem->release();
            break;
        case 4: /* sem_down [1 4 sem32] => [] */
            idx = in[1];
            if(objtbl[idx].type != OBJTYPE_SEM){
                abort();
            }
            objtbl[idx].obj.sem->acquire();
            break;
        case 5: /* mutex_alloc [1 5 recursive?] => [mtx32] */
            if(in[1] /* recursive? */){
                idx = newobj(OBJTYPE_RECURSIVE_MUTEX);
                objtbl[idx].obj.mtx_recursive = new std::recursive_mutex();
            }else{
                idx = newobj(OBJTYPE_MUTEX);
                objtbl[idx].obj.mtx = new std::mutex();
            }
            out[0] = idx;
            break;
        case 6: /* mutex_free [1 6 mtx32] => [] */
            idx = in[1];
            if(objtbl[idx].type == OBJTYPE_RECURSIVE_MUTEX){
                delete objtbl[idx].obj.mtx_recursive;
            }else if(objtbl[idx].type == OBJTYPE_MUTEX){
                delete objtbl[idx].obj.mtx;
            }else{
                abort();
            }
            delobj(idx);
            break;
        case 7: /* mutex_lock [1 7 mtx32] => [] */
            idx = in[1];
            if(objtbl[idx].type == OBJTYPE_RECURSIVE_MUTEX){
                objtbl[idx].obj.mtx_recursive->lock();
            }else if(objtbl[idx].type == OBJTYPE_MUTEX){
                objtbl[idx].obj.mtx->lock();
            }else{
                abort();
            }
            break;
        case 8: /* mutex_unlock [1 8 mtx32] => [] */
            idx = in[1];
            if(objtbl[idx].type == OBJTYPE_RECURSIVE_MUTEX){
                objtbl[idx].obj.mtx_recursive->unlock();
            }else if(objtbl[idx].type == OBJTYPE_MUTEX){
                objtbl[idx].obj.mtx->unlock();
            }else{
                abort();
            }
            break;
        default:
            abort();
            break;

    }
}

thread_local int my_thread_objid;
typedef uint32_t (*funcptr)(w2c_lin*, uint32_t);

static funcptr
getfunc(int idx){
    void* p;
    printf("Converting %d ...", idx);
    if(idx >= the_linux.w2c_T0.size){
        abort();
    }
    p = (void*)the_linux.w2c_T0.data[idx].func;
    printf(" %p\n", p);
    return (funcptr)p;
}

static uintptr_t
thr_trampoline(int objid){
    funcptr f;
    uint32_t ret;
    my_thread_objid = objid;
    f = getfunc(objtbl[objid].obj.thr.func32);
    ret = f(&the_linux, objtbl[objid].obj.thr.arg32);
    objtbl[objid].obj.thr.ret = ret;
    return ret; /* debug */
}

static void
mod_threads(uint64_t* in, uint64_t* out){
    int idx, idx2;
    uintptr_t res;
    switch(in[0]){
        case 1: /* thread_create [2 1 func32 arg32] => [thr32] */
            idx = newobj(OBJTYPE_THREAD);
            objtbl[idx].obj.thr.func32 = in[1];
            objtbl[idx].obj.thr.arg32 = in[2];
            objtbl[idx].obj.thr.thread = new std::thread(thr_trampoline, idx);
            out[0] = idx;
            break;
        case 2: /* thread_detach [2 2] => [] */
            idx = my_thread_objid;
            if(objtbl[idx].type != OBJTYPE_THREAD){
                abort();
            }
            objtbl[idx].obj.thr.thread->detach();
            break;
        case 3: /* thread_exit [2 3] => [] */
            idx = my_thread_objid;
            if(objtbl[idx].type != OBJTYPE_THREAD){
                abort();
            }
            printf("UNIMPL.\n");
            abort();
            break;
        case 4: /* thread_join [2 4 thr32] => [result] */
            idx = in[1];
            if(objtbl[idx].type != OBJTYPE_THREAD){
                abort();
            }
            out[0] = objtbl[idx].obj.thr.ret;
            delobj(idx);
            break;
        case 5: /* thread_self [2 5] => [thr32] */
            idx = my_thread_objid;
            if(objtbl[idx].type != OBJTYPE_THREAD){
                abort();
            }
            out[0] = idx;
            break;
        case 6: /* thread_equal [2 6 thr32 thr32] => [equ?] */
            idx = in[1];
            idx2 = in[2];
            if(objtbl[idx].type != OBJTYPE_THREAD){
                abort();
            }
            if(objtbl[idx2].type != OBJTYPE_THREAD){
                abort();
            }
            out[0] = (idx == idx2) ? 1 : 0;
            break;
        case 7: /* gettid [2 7] => [tid] */
            idx = my_thread_objid;
            if(objtbl[idx].type != OBJTYPE_THREAD){
                abort();
            }
            out[0] = idx;
            break;
        case 8: /* tls_alloc [2 8 func32] => [tlskey32] */
        case 9: /* tls_free [2 9 tlskey32] => [] */
        case 10: /* tls_set [2 10 tlskey32 ptr32] => [res] */
        case 11: /* tls_get [2 11 tlskey32] => [ptr32] */
        default:
            abort();
            break;
    }
}

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
    out = (uint64_t*)outp;

    mod = in[0];
    func = in[1];

    printf("CALL: %ld %ld \n", mod, func);

    switch(mod){
        case 1: /* syncobjects */
            mod_syncobjects(&in[1], out);
            break;
        case 2: /* threads */
            mod_threads(&in[1], out);
            break;
        case 0: /* Admin */
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
    int i;
    for(i=0;i!=MAX_HOSTOBJ;i++){
        objtbl[i].id = i;
        objtbl[i].type = OBJTYPE_FREE;
    }
    objtbl[0].type = OBJTYPE_DUMMY; /* Avoid 0 idx */
    wasm_rt_init();
    wasm2c_lin_instantiate(&the_linux, 0);
    
    w2c_lin_init(&the_linux);
    return 0;
}
