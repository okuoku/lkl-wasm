#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include "mplite.h"

#include <thread>
#include <mutex>
#include <condition_variable>
#include <semaphore>

/* Kernel */
#include "lin.h"
w2c_kernel the_linux;
thread_local w2c_kernel* my_linux;

/* Userproc */
#include "user.h"
w2c_user the_user;
thread_local w2c_user* my_user;
wasm_rt_funcref_table_t userfuncs;
uint32_t userdata;
uint32_t userstack;
uint32_t usertablebase;

/* Pool management */
uint8_t* mpool_base;
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

void*
pool_alloc(uintptr_t size){
    return mplite_malloc(&mpool, size);
}

void
pool_free(void* ptr){
    mplite_free(&mpool, ptr);
}

uint32_t
pool_lklptr(void* ptr){
    return (uintptr_t)((uint8_t*)ptr - mpool_base);
}

void*
pool_hostptr(uint32_t offs){
    return mpool_base + (uintptr_t)offs;
}


/* Objmgr */
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
#define MAX_MYTLS 128
std::mutex objmtx;
struct hostobj_s objtbl[MAX_HOSTOBJ];
thread_local uint32_t mytls[MAX_MYTLS];
thread_local int my_thread_objid;

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


/* Kernel <-> User */

const uint64_t WASM_PAGE_SIZE = (64*1024);

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

static uint32_t
newtask_process(void){
    return w2c_kernel_taskmgmt(my_linux, 1, 0);
}

static uint32_t
newtask_thread(void){
    return w2c_kernel_taskmgmt(my_linux, 2, 0);
}

static void
newtask_apply(uint32_t ctx){
    w2c_kernel_taskmgmt(my_linux, 3, ctx);
}

std::mutex instancemtx;
static void
newinstance(){
    wasm_rt_memory_t* mem;
    wasm_rt_funcref_t* newfuncref;
    uint32_t i;
    w2c_kernel* me;
    uint64_t currentpages;
    const uint64_t STACK_PAGES = 10;
    const uint64_t STACK_SIZE = STACK_PAGES * WASM_PAGE_SIZE;
    std::lock_guard<std::mutex> NN(instancemtx);

    /* Allocate thread stack (10 pages = 160KiB) */
    // FIXME: Insert guard page
    // FIXME: Leaks stack memory and instance
    mem = &the_linux.w2c_memory;
    currentpages = wasm_rt_grow_memory(mem, STACK_PAGES);

    /* Allocate new instance */
    me = (w2c_kernel*)malloc(sizeof(w2c_kernel));
    newfuncref = (wasm_rt_funcref_t*)malloc(sizeof(wasm_rt_funcref_t)*the_linux.w2c_T0.size);
    memcpy(newfuncref, the_linux.w2c_T0.data, sizeof(wasm_rt_funcref_t)*the_linux.w2c_T0.size);
    for(i=0;i!=the_linux.w2c_T0.size;i++){
        newfuncref[i].module_instance = me;
    }

    memcpy(me, &the_linux, sizeof(w2c_kernel));
    me->w2c_0x5F_stack_pointer = (currentpages + STACK_PAGES) * WASM_PAGE_SIZE - 256 /* Red zone + 128(unused) */;
    me->w2c_T0.max_size = me->w2c_T0.size;
    me->w2c_T0.data = newfuncref;
    //printf("New stack pointer = %d\n", me->w2c_0x5F_stack_pointer);

    my_linux = me;
}


uint32_t /* -errno */
runsyscall32(uint32_t no, uint32_t nargs, uint32_t in){
    return w2c_kernel_syscall(my_linux, no, nargs, in);
}

static uint32_t
debuggetpid(void){
    return runsyscall32(172 /* __NR_getpid */, 0, 0);
}

static uint32_t
debugdup3(uint32_t oldfd, uint32_t newfd, uint32_t flags){
    int32_t* buf;
    uint32_t ptr0;
    int32_t res;
    /* Assume the caller already have Linux context */
    buf = (int32_t*)pool_alloc(4*3);
    ptr0 = pool_lklptr(&buf[0]);
    buf[0] = oldfd;
    buf[1] = newfd;
    buf[2] = flags;
    res = runsyscall32(24 /* __NR_dup3 */, 3, ptr0);
    printf("debug dup3 = %d\n", res);
    pool_free(buf);
    return res;
}

static void
debugclose(uint32_t fd){
    int32_t* buf;
    uint32_t ptr0;
    int32_t res;
    /* Assume the caller already have Linux context */
    buf = (int32_t*)pool_alloc(4);
    ptr0 = pool_lklptr(&buf[0]);
    buf[0] = fd;
    res = runsyscall32(57 /* __NR_close */, 1, ptr0);
    printf("debug close(%d) = %d\n", fd, res);
    pool_free(buf);
    return;
}

static int kfd_stdout;
static int kfd_stderr;

/* User handlers */
uint32_t*
w2c_env_0x5F_table_base(struct w2c_env* bogus){
    return &usertablebase;
}

uint32_t*
w2c_env_0x5F_memory_base(struct w2c_env* bogus){
    return &userdata;
}

uint32_t*
w2c_env_0x5F_stack_pointer(struct w2c_env* bogus){
    return &userstack;
}

wasm_rt_funcref_table_t* 
w2c_env_0x5F_indirect_function_table(struct w2c_env* bogus){
    return &userfuncs;
}

wasm_rt_memory_t* 
w2c_env_memory(struct w2c_env* bogus){
    return &the_linux.w2c_memory;
}

uint32_t
w2c_env_wasmlinux_syscall32(struct w2c_env* env, uint32_t argc, uint32_t no,
                            uint32_t args){
    printf("(user) syscall = %d\n", no);
    return runsyscall32(no, argc, args);
}

static uint32_t
create_envblock(const char* argv[], char const* envp){
    size_t o, s, arrsize, total, argtotal, envpsize;
    int i;
    uint32_t ptr0;
    uint32_t* a;
    char* buf;
    /* [0] argc = envblock
     * [1] argv(0)
     * [2] argv(1)
     *   :
     * [argc] argv(argc) = 0
     * [argc+1] envp */

    /* Pass1: Calc bufsize */
    i = 0;
    argtotal = 0;
    while(argv[i]){
        argtotal += strnlen(argv[i], 1024*1024);
        argtotal ++; /* NUL */
        i++;
    }
    arrsize = i+2 /* terminator + envp */;
    envpsize = strnlen(envp, 1024*1024);
    envpsize++; /* NUL */

    total = arrsize*4 + argtotal + envpsize;
    buf = (char*)pool_alloc(total*4 + argtotal + envpsize);
    ptr0 = pool_lklptr(buf);
    memset(buf, 0, total);

    /* Pass2: Fill buffer */
    o = arrsize*4;
    a = (uint32_t*)buf;
    a[0] = arrsize - 2; /* argc */
    for(i=0;i!=a[0];i++){
        a[i+1] = pool_lklptr(buf + o); /* argv* */
        s = strnlen(argv[i], 1024*1024);
        memcpy(buf + o, argv[i], s);
        o += (s + 1);
    }
    memcpy(buf + o, envp, envpsize);
    return ptr0;
}

static void
thr_user(uint32_t procctx){
    uint32_t envblock;
    uint32_t ret;
    const char* argv[] = {
        "dummy", 0
    };
    const char* envp = "";
    /* Allocate linux context */
    newinstance();
    prepare_newthread();

    /* Assign user instance */
    my_user = &the_user;

    /* Assign process ctx */
    newtask_apply(procctx);

    /* Setup initial stdin/out */
    ret = debugdup3(kfd_stdout, 10, 0);
    printf("(user) stdout => 10 : %d\n", ret);
    ret = debugdup3(kfd_stderr, 11, 0);
    printf("(user) stderr => 11 : %d\n", ret);
    debugclose(kfd_stdout);
    debugclose(kfd_stderr);
    ret = debugdup3(10, 1, 0);
    printf("(user) 10 => stdout : %d\n", ret);
    ret = debugdup3(11, 2, 0);
    printf("(user) 11 => stderr : %d\n", ret);


    /* MUSL startup */
    envblock = create_envblock(argv, envp);
    /* Run usercode */
    // Raw init
    // ret = w2c_user_main(my_user, 0, 0);
    // printf("(user) ret = %d\n", ret);
    w2c_user_0x5Fstart_c(my_user, envblock);
    // FIXME: free envblock at exit()
}

static void
spawn_user(void){
    uint32_t procctx;
    std::thread* thr;

    /* Instantiate user module */
    userstack = pool_lklptr(pool_alloc(1024*1024));
    userdata = pool_lklptr(pool_alloc(wasm2c_user_max_env_memory));
    /* FIXME: calc max size */
    wasm_rt_allocate_funcref_table(&userfuncs, 1024, 1024);
    usertablebase = 0;
    printf("(user) data = %x\n", userdata);
    printf("(user) stack = %x\n", userstack);
    printf("(user) tablebase = %x\n", usertablebase);

    wasm2c_user_instantiate(&the_user, 0);

    /* fork */
    procctx = newtask_process();
    printf("procctx = %d\n", procctx);

    thr = new std::thread(thr_user, procctx);
    thr->detach();
}

/* Kernel handlers */


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

typedef uint32_t (*funcptr)(w2c_kernel*, uint32_t);
typedef void (*funcptr_cont)(w2c_kernel*);

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

static funcptr_cont
getfunc_cont(int idx){
    void* p;
    //printf("Converting %d ...", idx);
    if(idx >= the_linux.w2c_T0.size){
        abort();
    }
    p = (void*)the_linux.w2c_T0.data[idx].func;
    //printf(" %p\n", p);
    return (funcptr_cont)p;
}



struct {
    uint32_t func32_destructor;
    int used;
} tlsstate[MAX_MYTLS];

std::mutex tlsidmtx;

static uint32_t /* Key */
thr_tls_alloc(uint32_t destructor){
    std::lock_guard<std::mutex> NN(tlsidmtx);
    int i;
    /* Don't return 0 as TLS key */
    for(i=1;i!=MAX_MYTLS;i++){
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
    if(key >= MAX_MYTLS){
        abort();
    }
    tlsstate[key].used = 0;
}

static uint32_t
thr_tls_get(uint32_t key){
    if(key >= MAX_MYTLS){
        abort();
    }
    printf("TLS[%d]: %d -> %x\n",my_thread_objid,key,mytls[key]);
    return mytls[key];
}

static uint32_t
thr_tls_set(uint32_t key, uint32_t data){
    if(key >= MAX_MYTLS){
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
    uint32_t ret = 0;
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
            out[0] = thr_tls_get(in[1]);
            break;
        default:
            abort();
            break;
    }
}

static void
thr_debugprintthread(uint32_t fd, int ident){
    int32_t* buf;
    uint32_t ptr0, ptr1;
    int32_t res;
    char linebuf[2000];
    int i,r;
    const char* header;
    header = (ident == 0) ? "[stdout]" : "[stderr]";

    /* Allocate linux context */
    newinstance();
    prepare_newthread();

    /* Allocate syscall buffer */
    buf = (int32_t*)pool_alloc(3000);
    ptr0 = pool_lklptr(&buf[0]);
    ptr1 = pool_lklptr(&buf[3]);

    for(;;){
        buf[0] = fd;
        buf[1] = ptr1;
        buf[2] = 2000;
        res = runsyscall32(63 /* __NR_read */, 3, ptr0);
        printf("res = %d (from: %d, %x)\n", res, my_thread_objid, mytls[1]);
        if(res < 0){
            break;
        }
        if(res > 2000){
            printf("???\n");
            abort();
        }
        memcpy(linebuf, (void*)&buf[3], res);
        linebuf[res] = 0;
        r = 0;
        for(i=0;i!=res;i++){
            switch(linebuf[i]){
                case '\n':
                    linebuf[i] = 0;
                    printf("%s: %s\n", header,
                           (char*)&linebuf[r]);
                    r = i+1;
                    break;
                default:
                    break;
            }
        }
        if(r<i){
            printf("%s: %s\n", header, (char*)&linebuf[r]);
        }
    }

    pool_free(buf);
    thr_tls_cleanup();
}

static void
debugwrite(uint32_t fd, const char* data, size_t len){
    int32_t* buf;
    uint32_t ptr0, ptr1;
    int32_t res;
    /* Assume the caller already have Linux context */
    buf = (int32_t*)pool_alloc(4*3+len);
    ptr0 = pool_lklptr(&buf[0]);
    ptr1 = pool_lklptr(&buf[3]);
    memcpy((char*)&buf[3], data, len);
    buf[0] = fd;
    buf[1] = ptr1;
    buf[2] = len;
    res = runsyscall32(64 /* __NR_write */, 3, ptr0);
    printf("write res = %d\n", res);
    pool_free(buf);
}

void
spawn_debugiothread(void){
    int32_t* buf;
    uint32_t ptr0, ptr1;
    int32_t ret;
    /* Allocate syscall buffer */
    buf = (int32_t*)pool_alloc(sizeof(int32_t)*32);

    for(int i=0;i!=2;i++){
        std::thread* thr;
        /* Generate Pipe inside kernel */
        ptr0 = pool_lklptr(&buf[0]);
        ptr1 = pool_lklptr(&buf[2]);
        buf[0] = ptr1; /* pipefd[2] */
        buf[1] = 0; /* flags */

        ret = runsyscall32(59 /* pipe2 */, 2, ptr0);
        printf("Ret: %d, %d, %d\n", ret, buf[2], buf[3]);

        /* Spawn handler */
        thr = new std::thread(thr_debugprintthread, buf[2], i);
        thr->detach();

        if(i == 0){
            kfd_stdout = buf[3];
        }else{
            kfd_stderr = buf[3];
        }
    }
    pool_free(buf);
}

void
mod_memorymgr(uint64_t* in, uint64_t* out){
    void* ptr;
    switch(in[0]){
        case 1: /* mem_alloc [3 1 size] => [ptr32] */
            ptr = pool_alloc(in[1]);
            out[0] = pool_lklptr(ptr);
            printf("malloc: %p (offs: %p) %ld\n", ptr, out[0], in[1]);
            break;
        case 2: /* mem_free [3 2 ptr32] => [] */
            pool_free(pool_hostptr(in[1]));
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
            mem = &the_linux.w2c_memory;
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

    {
        std::unique_lock<std::mutex> NN(*mtx);
        for(;;){
            wait_for = objtbl[objid].obj.timer.wait_for;
            objtbl[objid].obj.timer.wait_for = UINT64_MAX-1;
            if(wait_for == UINT64_MAX){
                /* Dispose timer */
                break;
            }else if(wait_for == (UINT64_MAX-1)){
                /* No request at this time. */
                cv->wait(NN);
            }else{
                std::cv_status s;
                /* wait and fire */
                //printf("Wait: %ld\n",wait_for);
                s = cv->wait_for(NN, std::chrono::nanoseconds(wait_for));
                if(s == std::cv_status::timeout){
                    //printf("Fire: %d\n",objtbl[objid].obj.timer.func32);
                    mtx->unlock();
                    ret = f(my_linux, arg32);
                    mtx->lock();
                    //printf("Done: %d\n",objtbl[objid].obj.timer.func32);
                }else{
                    if(objtbl[objid].obj.timer.wait_for == UINT64_MAX-1){
                        printf("Spurious wakeup!: %ld again\n", wait_for);
                        objtbl[objid].obj.timer.wait_for = wait_for;
                    }else{
                        printf("Rearm: %ld\n", objtbl[objid].obj.timer.wait_for);
                    }
                }
            }
        }
    }

    printf("Dispose timer %d\n", objid);
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
            //printf("Oneshot timer: %d %ld\n",idx, in[2]);
            {
                std::unique_lock<std::mutex> NN(*objtbl[idx].obj.timer.mtx);
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
                objtbl[idx].obj.timer.wait_for = UINT64_MAX;
                objtbl[idx].obj.timer.cv->notify_one();
            }
            break;
        default:
            abort();
            break;
    }
}

class guardian {public: uintptr_t ident;};

static void
mod_ctx(uint64_t* in, uint64_t* out){
    guardian* gp;
    uintptr_t* p;
    funcptr_cont f;
    switch(in[0]){
        case 1: /* jmp_buf_set [5 1 ptr32 func32 sizeof_jmpbuf] => [] */
            gp = new guardian();
            gp->ident = (uintptr_t)gp;
            p = (uintptr_t*)pool_hostptr(in[1]);
            *p = (uintptr_t)(gp);
            f = getfunc_cont(in[2]);
            try {
                //printf("Run: %lx %p\n", in[1], gp);
                f(my_linux);
            } catch (guardian& gg) {
                if(gg.ident != (uintptr_t)gp){
                    throw gg;
                }else{
                    delete gp;
                }
            }
            break;
        case 2: /* jmp_buf_longjmp [5 2 ptr32 val32] => NORETURN */
            p = (uintptr_t*)pool_hostptr(in[1]);
            gp = (guardian*)(*p);
            //printf("Throw: %lx %p\n", in[1], gp);
            throw *gp;
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
    mem = &the_linux.w2c_memory;

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
            mod_ctx(&in[1], out);
            break;
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
    uint32_t mpool_start;

    /* Init objtbl */
    for(i=0;i!=MAX_HOSTOBJ;i++){
        objtbl[i].id = i;
        objtbl[i].type = OBJTYPE_FREE;
    }
    objtbl[0].type = OBJTYPE_DUMMY; /* Avoid 0 idx */
    wasm_rt_init();
    wasm2c_kernel_instantiate(&the_linux, 0);

    /* Init TLS slots */
    for(i=0;i!=MAX_MYTLS;i++){
        tlsstate[i] = { 0 };
    }
    
    my_linux = &the_linux;
    prepare_newthread();

    /* Init memory pool */
    mem = &the_linux.w2c_memory;
    startpages = wasm_rt_grow_memory(mem, 4096 * 4);
    maxpages = startpages + 4096 * 4;

    printf("memmgr region = ptr: %p pages: %ld - %ld\n", mem->data, 
           startpages, maxpages);

    mpool_start = (startpages * WASM_PAGE_SIZE);
    mpool_base = mem->data;
    mplite_init(&mpool, mem->data + mpool_start,
                (maxpages - startpages) * WASM_PAGE_SIZE,
                64, &mpool_lockimpl);
    
    /* Initialize kernel */
    w2c_kernel_init(&the_linux);

    /* Create debug I/O thread */
    spawn_debugiothread();

    printf("(init) pid = %d\n", debuggetpid());

    /* Create user program */
    spawn_user();

    /* Sleep */
    for(;;){
        std::this_thread::sleep_for(std::chrono::seconds(1));
    }

    return 0;
}
