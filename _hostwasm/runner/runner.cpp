#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include "lin.h"
#include "mplite.h"

#include <thread>
#include <mutex>
#include <condition_variable>
#include <semaphore>

w2c_lin the_linux;
thread_local w2c_lin* my_linux;

const uint64_t WASM_PAGE_SIZE = (64*1024);

std::mutex instancemtx;
static void
newinstance(){
    wasm_rt_memory_t* mem;
    wasm_rt_funcref_t* newfuncref;
    uint32_t i;
    w2c_lin* me;
    uint64_t currentpages;
    const uint64_t STACK_PAGES = 10;
    const uint64_t STACK_SIZE = STACK_PAGES * WASM_PAGE_SIZE;
    std::lock_guard<std::mutex> NN(instancemtx);

    /* Allocate thread stack (10 pages = 160KiB) */
    // FIXME: Insert guard page
    // FIXME: Leaks stack memory and instance
    mem = w2c_lin_memory(&the_linux);
    currentpages = wasm_rt_grow_memory(mem, STACK_PAGES);

    /* Allocate new instance */
    me = (w2c_lin*)malloc(sizeof(w2c_lin));
    newfuncref = (wasm_rt_funcref_t*)malloc(sizeof(wasm_rt_funcref_t)*the_linux.w2c_T0.size);
    memcpy(newfuncref, the_linux.w2c_T0.data, sizeof(wasm_rt_funcref_t)*the_linux.w2c_T0.size);
    for(i=0;i!=the_linux.w2c_T0.size;i++){
        newfuncref[i].module_instance = me;
    }

    memcpy(me, &the_linux, sizeof(w2c_lin));
    me->w2c_0x5F_stack_pointer = (currentpages + STACK_PAGES) * WASM_PAGE_SIZE - STACK_SIZE + 256 /* Red zone + 128(unused) */;
    me->w2c_T0.max_size = me->w2c_T0.size;
    me->w2c_T0.data = newfuncref;
    //printf("New stack pointer = %d\n", me->w2c_0x5F_stack_pointer);

    my_linux = me;
}


enum objtype{
    OBJTYPE_DUMMY = 0,
    OBJTYPE_FREE = 1,
    OBJTYPE_SEM = 2,
    OBJTYPE_MUTEX = 3,
    OBJTYPE_RECURSIVE_MUTEX = 4,
    OBJTYPE_THREAD = 5,
    OBJTYPE_TIMER = 6
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
        struct {
            uint64_t wait_for;
            uintptr_t func32;
            uintptr_t arg32;
            std::condition_variable* cv;
            std::mutex* mtx;
            std::thread* thread;
            int running;
        } timer;
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
    //printf("Converting %d ...", idx);
    if(idx >= the_linux.w2c_T0.size){
        abort();
    }
    p = (void*)the_linux.w2c_T0.data[idx].func;
    //printf(" %p\n", p);
    return (funcptr)p;
}


#define MAX_MYTLS 128

struct {
    uint32_t func32_destructor;
    int used;
} tlsstate[MAX_MYTLS];

std::mutex tlsidmtx;
thread_local uint32_t mytls[MAX_MYTLS];

static uint32_t /* Key */
thr_tls_alloc(uint32_t destructor){
    std::lock_guard<std::mutex> NN(tlsidmtx);
    int i;
    for(i=0;i!=MAX_MYTLS;i++){
        if(tlsstate[i].used == 0){
            tlsstate[i].used = 1;
            tlsstate[i].func32_destructor = destructor;
            return i;
        }
    }
    abort();
    return 0; /* unreachable */
}

static void
thr_tls_free(uint32_t key){
    std::lock_guard<std::mutex> NN(tlsidmtx);
    if(key > MAX_MYTLS){
        abort();
    }
    tlsstate[key].used = 0;
}

static uint32_t
thr_tls_get(uint32_t key){
    if(key > MAX_MYTLS){
        abort();
    }
    return mytls[key];
}

static uint32_t
thr_tls_set(uint32_t key, uint32_t data){
    if(key > MAX_MYTLS){
        abort();
    }
    mytls[key] = data;
    return 0;
}

static void
thr_tls_cleanup(void){
    int i,runloop;
    funcptr f;
    runloop = 1;
    while(runloop){
        runloop = 0;
        for(i=0;i!=MAX_MYTLS;i++){
            if(mytls[i] != 0){
                std::lock_guard<std::mutex> NN(tlsidmtx);
                uint32_t funcid;
                funcid = tlsstate[i].func32_destructor;
                if(funcid != 0){
                    f = getfunc(objtbl[funcid].obj.thr.func32);
                    (void)f(my_linux, mytls[i]);
                    mytls[i] = 0;
                    runloop = 1;
                }
            }
        }
    }
}

class thr_exit {};

static uintptr_t
thr_trampoline(int objid){
    funcptr f;
    uint32_t ret;
    try {
        newinstance();
        memset(mytls, 0, sizeof(mytls));
        my_thread_objid = objid;
        f = getfunc(objtbl[objid].obj.thr.func32);
        ret = f(my_linux, objtbl[objid].obj.thr.arg32);
        thr_tls_cleanup();
        objtbl[objid].obj.thr.ret = ret;
    } catch (thr_exit &req) {
        printf("Exiting thread.\n");
        thr_tls_cleanup();
    }
    return ret; /* debug */
}

static void
prepare_newthread(void){
    int idx;
    /* Allocate initial thread */
    memset(mytls, 0, sizeof(mytls));
    idx = newobj(OBJTYPE_THREAD);
    objtbl[idx].obj.thr.func32 = 0;
    objtbl[idx].obj.thr.arg32 = 0;
    objtbl[idx].obj.thr.thread = nullptr;
    my_thread_objid = idx;
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
            {
                thr_exit e;
                throw e;
            }
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
            out[0] = thr_tls_alloc(in[1]);
            break;
        case 9: /* tls_free [2 9 tlskey32] => [] */
            thr_tls_free(in[1]);
            break;
        case 10: /* tls_set [2 10 tlskey32 ptr32] => [res] */
            out[0] = thr_tls_set(in[1], in[2]);
            break;
        case 11: /* tls_get [2 11 tlskey32] => [ptr32] */
            out[1] = thr_tls_get(in[1]);
            break;
        default:
            abort();
            break;
    }
}

uintptr_t mpool_offset;
mplite_t mpool;
std::mutex mtxpool;

static int
pool_acquire(void* bogus){
    (void) bogus;
    mtxpool.lock();
    return 0;
}

static int
pool_release(void* bogus){
    (void) bogus;
    mtxpool.unlock();
    return 0;
}
const mplite_lock_t mpool_lockimpl = {
    .arg = 0,
    .acquire = pool_acquire,
    .release = pool_release,
};

void
mod_memorymgr(uint64_t* in, uint64_t* out){
    uintptr_t ptr;
    switch(in[0]){
        case 1: /* mem_alloc [3 1 size] => [ptr32] */
            ptr = (uintptr_t)mplite_malloc(&mpool, in[1]);
            out[0] = ptr - mpool_offset;
            printf("malloc: %p (offs: %p) %ld\n", ptr, out[0], in[1]);
            break;
        case 2: /* mem_free [3 2 ptr32] => [] */
            ptr = in[1] + mpool_offset;
            mplite_free(&mpool, (void*)ptr);
            break;
        default:
            abort();
            break;
    }
}

void
mod_admin(uint64_t* in, uint64_t* out){
    char* buf;
    wasm_rt_memory_t* mem;
    switch(in[0]){
        case 1: /* print [0 1 str len] => [] */
            mem = w2c_lin_memory(&the_linux);
            buf = (char*)malloc(in[2] + 1);
            buf[in[2]] = 0;
            memcpy(buf, mem->data + in[1], in[2]);
            puts(buf);
            break;
        case 2: /* panic [0 2] => HALT */
            printf("panic.\n");
            abort();
        default:
            abort();
            break;
    }
}

static uint64_t
current_ns(void){
    std::chrono::nanoseconds ns;
    ns = std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::system_clock::now().time_since_epoch());
    return ns.count();
}

static void
thr_timer(int objid){
    funcptr f;
    uint32_t arg32;
    uint32_t ret;
    uint64_t wait_for;
    std::mutex* mtx;
    std::condition_variable* cv;

    newinstance();
    prepare_newthread();

    f = getfunc(objtbl[objid].obj.timer.func32);
    arg32 = objtbl[objid].obj.timer.arg32;

    cv = objtbl[objid].obj.timer.cv;
    mtx = objtbl[objid].obj.timer.mtx;

    for(;;){
        std::unique_lock<std::mutex> NN(*mtx);
        objtbl[objid].obj.timer.running = 0;
        cv->wait(NN);
        wait_for = objtbl[objid].obj.timer.wait_for;
        if(wait_for == UINT64_MAX){
            /* Dispose timer */
            break;
        }else{
            /* wait and fire */
            objtbl[objid].obj.timer.running = 1;
            NN.unlock();
            std::this_thread::sleep_for(std::chrono::nanoseconds(wait_for));
            ret = f(my_linux, arg32);
        }
    }

    delete mtx;
    delete cv;
    delobj(objid);
    // FIXME: Free instance
    return;
}

static void
mod_timer(uint64_t* in, uint64_t* out){
    int idx;
    uint64_t delta;
    switch(in[0]){

        case 1: /* time [4 1] => [time64] */
            out[0] = current_ns();
            break;
        case 2: /* timer_alloc [4 2 func32 arg32] => [timer32] */
            idx = newobj(OBJTYPE_TIMER);
            objtbl[idx].obj.timer.func32 = in[1];
            objtbl[idx].obj.timer.arg32 = in[2];
            objtbl[idx].obj.timer.thread = new std::thread(thr_timer, idx);
            objtbl[idx].obj.timer.mtx = new std::mutex();
            objtbl[idx].obj.timer.cv = new std::condition_variable();
            objtbl[idx].obj.timer.wait_for = UINT64_MAX-1;
            objtbl[idx].obj.timer.running = 0;
            objtbl[idx].obj.timer.thread->detach();
            out[0] = idx;
            break;
        case 3: /* timer_set_oneshot [4 3 timer32 delta64] => [res] */
            idx = in[1];
            if(objtbl[idx].type != OBJTYPE_TIMER){
                abort();
            }
            printf("Oneshot timer: %d %ld\n",idx, in[2]);
            {
                std::unique_lock<std::mutex> NN(*objtbl[idx].obj.timer.mtx);
                if(objtbl[idx].obj.timer.running){
                    printf("BUG BUG: Trying to wakeup multiple times.\n");
                    abort();
                }
                objtbl[idx].obj.timer.wait_for = in[2];
                objtbl[idx].obj.timer.cv->notify_one();
            }
            out[0] = 0;
            break;
        case 4: /* timer_free [4 4 timer32] => [] */
            idx = in[1];
            if(objtbl[idx].type != OBJTYPE_TIMER){
                abort();
            }
            {
                std::unique_lock<std::mutex> NN(*objtbl[idx].obj.timer.mtx);
                if(objtbl[idx].obj.timer.running){
                    printf("BUG BUG: Trying to dispose running timer.\n");
                    abort();
                }
                objtbl[idx].obj.timer.wait_for = UINT64_MAX;
                objtbl[idx].obj.timer.cv->notify_one();
            }
            break;
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
    //printf("CALL: %ld %ld \n", mod, func);

    switch(mod){
        case 0: /* Admin */
            mod_admin(&in[1], out);
            break;
        case 1: /* syncobjects */
            mod_syncobjects(&in[1], out);
            break;
        case 2: /* threads */
            mod_threads(&in[1], out);
            break;
        case 3: /* memory mgr */
            mod_memorymgr(&in[1], out);
            break;
        case 4: /* timer */
            mod_timer(&in[1], out);
            break;
        case 5: /* context */
        default:
            printf("Unkown request: %ld %ld \n", mod, func);
            abort();
            return;
    }
}

int
main(int ac, char** av){
    int i;
    int idx;
    wasm_rt_memory_t* mem;
    uint64_t startpages;
    uint64_t maxpages;

    /* Init objtbl */
    for(i=0;i!=MAX_HOSTOBJ;i++){
        objtbl[i].id = i;
        objtbl[i].type = OBJTYPE_FREE;
    }
    objtbl[0].type = OBJTYPE_DUMMY; /* Avoid 0 idx */
    wasm_rt_init();
    wasm2c_lin_instantiate(&the_linux, 0);

    /* Init TLS slots */
    for(i=0;i!=MAX_MYTLS;i++){
        tlsstate[i] = { 0 };
    }
    
    my_linux = &the_linux;
    prepare_newthread();

    /* Init memory pool */
    mem = w2c_lin_memory(&the_linux);
    startpages = wasm_rt_grow_memory(mem, 4096 * 4);
    maxpages = startpages + 4096 * 4;

    printf("memmgr region = ptr: %p pages: %ld - %ld\n", mem->data, 
           startpages, maxpages);

    mpool_offset = startpages * 64 * 1024;
    mplite_init(&mpool, mem->data + mpool_offset,
                (maxpages - startpages) * 64 * 1024,
                64, &mpool_lockimpl);
    
    w2c_lin_init(&the_linux);
    return 0;
}
