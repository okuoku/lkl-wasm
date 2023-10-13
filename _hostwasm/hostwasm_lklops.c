#include <stdint.h>
#include <stddef.h>
#include <uapi/asm/host_ops.h> // Under arch/lkl/include

__attribute__((import_module("env"), import_name("nccc_call64"))) void nccc_call64(uint64_t* in, uint64_t* out);

/* 0: admin */
/* print [0 1 str len] => [] */
/* panic [0 2] => HALT */

static void
host_print(const char* str, int len){
    /* print [0 1 str len] => [] */
    uint64_t in[4];
    in[0] = 0;
    in[1] = 1;
    in[2] = (uint64_t)(uintptr_t)str;
    in[3] = (uint64_t)len;
    nccc_call64(in, 0);
}

static void
host_panic(void){
    /* panic [0 2] => HALT */
    uint64_t in[2];
    in[0] = 0;
    in[1] = 2;
    nccc_call64(in, 0);
}

/* 1: syncobjects */
/* sem_alloc [1 1 count] => [sem32] */
/* sem_free [1 2 sem32] => [] */
/* sem_up [1 3 sem32] => [] */
/* sem_down [1 4 sem32] => [] */
/* mutex_alloc [1 5 recursive?] => [mtx32] */
/* mutex_free [1 6 mtx32] => [] */
/* mutex_lock [1 7 mtx32] => [] */
/* mutex_unlock [1 8 mtx32] => [] */

static struct lkl_sem*
host_sem_alloc(int count){
    /* sem_alloc [1 1 count] => [sem32] */
    uint64_t in[3];
    uint64_t out[1];
    in[0] = 1;
    in[1] = 1;
    in[2] = count;
    nccc_call64(in, out);
    return (struct lkl_sem*)(uintptr_t)out[0];
}

static void
host_sem_free(struct lkl_sem* sem){
    /* sem_free [1 2 sem32] => [] */
    uint64_t in[3];
    in[0] = 1;
    in[1] = 2;
    in[2] = (uintptr_t)sem;
    nccc_call64(in, 0);
}

static void
host_sem_up(struct lkl_sem* sem){
    /* sem_up [1 3 sem32] => [] */
    uint64_t in[3];
    in[0] = 1;
    in[1] = 3;
    in[2] = (uintptr_t)sem;
    nccc_call64(in, 0);
}

static void
host_sem_down(struct lkl_sem* sem){
    /* sem_down [1 4 sem32] => [] */
    uint64_t in[3];
    in[0] = 1;
    in[1] = 4;
    in[2] = (uintptr_t)sem;
    nccc_call64(in, 0);
}

static struct lkl_mutex*
host_mutex_alloc(int recursive){
    /* mutex_alloc [1 5 recursive?] => [mtx32] */
    uint64_t in[3];
    uint64_t out[1];
    in[0] = 1;
    in[1] = 5;
    in[2] = recursive;
    nccc_call64(in, out);
    return (struct lkl_mutex*)(uintptr_t)out[0];
}

static void
host_mutex_free(struct lkl_mutex* mutex){
    /* mutex_free [1 6 mtx32] => [] */
    uint64_t in[3];
    in[0] = 1;
    in[1] = 6;
    in[2] = (uintptr_t)mutex;
    nccc_call64(in, 0);
}

static void
host_mutex_lock(struct lkl_mutex* mutex){
    /* mutex_lock [1 7 mtx32] => [] */
    uint64_t in[3];
    in[0] = 1;
    in[1] = 7;
    in[2] = (uintptr_t)mutex;
    nccc_call64(in, 0);
}

static void
host_mutex_unlock(struct lkl_mutex* mutex){
    /* mutex_unlock [1 8 mtx32] => [] */
    uint64_t in[3];
    in[0] = 1;
    in[1] = 8;
    in[2] = (uintptr_t)mutex;
    nccc_call64(in, 0);
}

/* 2: threads */
/* thread_create [2 1 func32 arg32] => [thr32] */
/* thread_detach [2 2] => [] */
/* thread_exit [2 3] => [] */
/* thread_join [2 4 thr32] => [result] */
/* thread_self [2 5] => [thr32] */
/* thread_equal [2 6 thr32 thr32] => [equ?] */
/* gettid [2 7] => [tid] */
/* tls_alloc [2 8 func32] => [tlskey32] */
/* tls_free [2 9 tlskey32] => [] */
/* tls_set [2 10 tlskey32 ptr32] => [res] */
/* tls_get [2 11 tlskey32] => [ptr32] */

static lkl_thread_t
host_thread_create(void (*f)(void*), void* arg){
    /* thread_create [2 1 func32 arg32] => [thr32] */
    uint64_t in[4];
    uint64_t out[1];
    in[0] = 2;
    in[1] = 1;
    in[2] = (uintptr_t)f;
    in[3] = (uintptr_t)arg;
    nccc_call64(in, out);
    return (lkl_thread_t)out[0];
}

static void
host_thread_detach(void){
    /* thread_detach [2 2] => [] */
    uint64_t in[2];
    in[0] = 2;
    in[1] = 2;
    nccc_call64(in, 0);
}

static void
host_thread_exit(void){
    /* thread_exit [2 3] => NORETURN */
    uint64_t in[2];
    in[0] = 2;
    in[1] = 3;
    nccc_call64(in, 0);
}

static int
host_thread_join(lkl_thread_t tid){
    /* thread_join [2 4 thr32] => [result] */
    uint64_t in[3];
    uint64_t out[1];
    in[0] = 2;
    in[1] = 4;
    in[2] = (uintptr_t)tid;
    nccc_call64(in, out);
    return out[0];
}

static lkl_thread_t
host_thread_self(void){
    /* thread_self [2 5] => [thr32] */
    uint64_t in[2];
    uint64_t out[1];
    in[0] = 2;
    in[1] = 5;
    nccc_call64(in, out);
    return out[0];
}

static int
host_thread_equal(lkl_thread_t a, lkl_thread_t b){
    /* thread_equal [2 6 thr32 thr32] => [equ?] */
    uint64_t in[4];
    uint64_t out[1];
    in[0] = 2;
    in[1] = 6;
    in[2] = (uintptr_t)a;
    in[3] = (uintptr_t)b;
    nccc_call64(in, out);
    return out[0];
}

/* thread_stack */

static struct lkl_tls_key*
host_tls_alloc(void (*destructor)(void*)){
    /* tls_alloc [2 8 func32] => [tlskey32] */
    uint64_t in[3];
    uint64_t out[1];
    in[0] = 2;
    in[1] = 8;
    in[2] = (uintptr_t)destructor;
    nccc_call64(in, out);
    return (struct lkl_tls_key*)(uintptr_t)out[0];
}

static void
host_tls_free(struct lkl_tls_key* key){
    /* tls_free [2 9 tlskey32] => [] */
    uint64_t in[3];
    in[0] = 2;
    in[1] = 9;
    in[2] = (uintptr_t)key;
    nccc_call64(in, 0);
}

static int
host_tls_set(struct lkl_tls_key* key, void* data){
    /* tls_set [2 10 tlskey32 ptr32] => [res] */
    uint64_t in[4];
    uint64_t out[1];
    in[0] = 2;
    in[1] = 10;
    in[2] = (uintptr_t) key;
    in[3] = (uintptr_t) data;
    nccc_call64(in, out);
    return out[0];
}

static void*
host_tls_get(struct lkl_tls_key* key){
    /* tls_get [2 11 tlskey32] => [ptr32] */
    uint64_t in[3];
    uint64_t out[1];
    in[0] = 2;
    in[1] = 11;
    in[2] = (uintptr_t) key;
    nccc_call64(in, out);
    return (struct lkl_tls_key*)(uintptr_t)out[0];
}

/* 3:memory mgr */
/* mem_alloc [3 1 size] => [ptr32] */
/* mem_free [3 2 ptr32] => [] */

static void*
host_mem_alloc(unsigned long a){
    /* mem_alloc [3 1 size] => [ptr32] */
    uint64_t in[3];
    uint64_t out[1];
    in[0] = 3;
    in[1] = 1;
    in[2] = a;
    nccc_call64(in, out);
    return (void*)(uintptr_t)out[0];
}

static void
host_mem_free(void* p){
    /* mem_free [3 2 ptr32] => [] */
    uint64_t in[3];
    in[0] = 3;
    in[1] = 2;
    in[2] = (uintptr_t)p;
    nccc_call64(in, 0);
}

/* page_alloc */
/* page_free */

/* 4:timer */
/* time [4 1] => [time64] */
/* timer_alloc [4 2 func32 arg32] => [timer32] */
/* timer_set_oneshot [4 3 timer32 delta64 => [res] */
/* timer_free [4 4 timer32] => [] */

static unsigned long long
host_time(void){
    /* time [4 1] => [time64] */
    uint64_t in[2];
    uint64_t out[1];
    in[0] = 4;
    in[1] = 1;
    nccc_call64(in, out);
    return out[0];
}

static void*
host_timer_alloc(void (*fn)(void*), void* arg){
    /* timer_alloc [4 2 func32 arg32] => [timer32] */
    uint64_t in[4];
    uint64_t out[1];
    in[0] = 4;
    in[1] = 2;
    in[2] = (uintptr_t) fn;
    in[3] = (uintptr_t) arg;
    nccc_call64(in, out);
    return (void*)(uintptr_t)out[0];
}

static int
host_timer_set_oneshot(void* timer, unsigned long delta){
    /* timer_set_oneshot [4 3 timer32 delta64] => [res] */
    uint64_t in[4];
    uint64_t out[1];
    in[0] = 4;
    in[1] = 3;
    in[2] = (uintptr_t)timer;
    in[3] = delta;
    nccc_call64(in, out);
    return out[0];
}

static void
host_timer_free(void* timer){
    /* timer_free [4 4 timer32] => [] */
    uint64_t in[3];
    in[0] = 4;
    in[1] = 4;
    in[2] = (uintptr_t) timer;
    nccc_call64(in, 0);
}

/* ioremap */
/* iomem_access */

static long
host_gettid(void){
    /* gettid [2 7] => [tid] */
    uint64_t in[2];
    uint64_t out[1];
    in[0] = 2;
    in[1] = 7;
    nccc_call64(in, out);
    return out[0];
}

/* 5:context */
/* jmp_buf_set [5 1 ptr32 func32 sizeof_jmpbuf] => [] */
/* jmp_buf_longjmp [5 2 ptr32 val32] => NORETURN */

static void
host_jmp_buf_set(struct lkl_jmp_buf* jmpb, void (*f)(void)){
    /* jmp_buf_set [5 1 ptr32 func32 sizeof_jmpbuf] => [] */
    _Static_assert(sizeof(struct lkl_jmp_buf) >= 128, "jmp_buf size check");
    uint64_t in[5];
    in[0] = 5;
    in[1] = 1;
    in[2] = (uintptr_t)jmpb;
    in[3] = (uintptr_t)f;
    in[4] = sizeof(struct lkl_jmp_buf);
    nccc_call64(in, 0);
}

static void
host_jmp_buf_longjmp(struct lkl_jmp_buf* jmpb, int val){
    /* jmp_buf_longjmp [5 2 ptr32 val32] => NORETURN */
    uint64_t in[4];
    in[0] = 5;
    in[1] = 2;
    in[2] = (uintptr_t) jmpb;
    in[3] = val;
    nccc_call64(in, 0);
}

void *memset(void *s, int c, size_t n);
void *memcpy(void *dest, const void *src, size_t n);

static void*
host_memcpy(void* dest, const void* src, unsigned long count){
    return memcpy(dest, src, count);
}

static void*
host_memset(void* s, int c, unsigned long count){
    return memset(s, c, count);
}

/* mmap */
/* munmap */

/* pci_ops */

const struct lkl_host_operations host_lkl_ops = (struct lkl_host_operations){
    .virtio_devices = 0,
        .print = host_print,
        .panic = host_panic,
        .sem_alloc = host_sem_alloc,
        .sem_free = host_sem_free,
        .sem_up = host_sem_up,
        .sem_down = host_sem_down,
        .mutex_alloc = host_mutex_alloc,
        .mutex_free = host_mutex_free,
        .mutex_lock = host_mutex_lock,
        .mutex_unlock = host_mutex_unlock,
        .thread_create = host_thread_create,
        .thread_detach= host_thread_detach,
        .thread_exit = host_thread_exit,
        .thread_join = host_thread_join,
        .thread_self = host_thread_self,
        .thread_equal = host_thread_equal,
        .thread_stack = 0,
        .tls_alloc = host_tls_alloc,
        .tls_free = host_tls_free,
        .tls_set = host_tls_set,
        .tls_get = host_tls_get,
        .mem_alloc = host_mem_alloc,
        .mem_free = host_mem_free,
        .page_alloc = 0,
        .page_free = 0,
        .time = host_time,
        .timer_alloc = host_timer_alloc,
        .timer_set_oneshot = host_timer_set_oneshot,
        .timer_free = host_timer_free,
        .ioremap = 0,
        .iomem_access = 0,
        .gettid = host_gettid,
        .jmp_buf_set = host_jmp_buf_set,
        .jmp_buf_longjmp = host_jmp_buf_longjmp,
        .memcpy = host_memcpy,
        .memset = host_memset,
        .mmap = 0,
        .munmap = 0,
        .pci_ops = 0
};


void*
lklhost_getops(void){
    return &host_lkl_ops;
}
