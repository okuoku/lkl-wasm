#include <stdint.h>
#include <uapi/asm/host_ops.h> // Under arch/lkl/include

__attribute__((import_module("env"), import_name("nccc_call64"))) void nccc_call64(uint64_t* in, uint64_t* out);

/* 0: admin */
/* print [0 1 str len] => [] */
/* panic [0 2] => HALT */
static void
host_print(const char* str, int len){
    uint64_t in[2];
    in[0] = (uint64_t)(uintptr_t)str;
    in[1] = (uint64_t)len;
    nccc_call64(in, 0);
}

static void
host_panic(void){
    uint64_t in[2];
    in[0] = 0;
    in[1] = 2;
    nccc_call64(in, 0);
}

static struct lkl_sem*
host_sem_alloc(int count){
}

static void
host_sem_free(struct lkl_sem* sem){
}

static void
host_sem_up(struct lkl_sem* sem){
}

static void
host_sem_down(struct lkl_sem* sem){
}

static struct lkl_mutex*
host_mutex_alloc(int recursive){
}

static void
host_mutex_free(struct lkl_mutex* mutex){
}

static void
host_mutex_lock(struct lkl_mutex* mutex){
}

static void
host_mutex_unlock(struct lkl_mutex* mutex){
}

static lkl_thread_t
host_thread_create(void (*f)(void*), void* arg){
}

static void
host_thread_detach(void){
}

static void
host_thread_exit(void){
}

static int
host_thread_join(lkl_thread_t tid){
}

static lkl_thread_t
host_thread_self(void){
}

static int
host_thread_equal(lkl_thread_t a, lkl_thread_t b){
}

/* thread_stack */

static struct lkl_tls_key*
host_tls_alloc(void (*destructor)(void*)){
}

static void
host_tls_free(struct lkl_tls_key* key){
}

static int
host_tls_set(struct lkl_tls_key* key, void* data){
}

static void*
host_tls_get(struct lkl_tls_key* key){
}

static void*
host_mem_alloc(unsigned long a){
}

static void
host_mem_free(void* p){
}

/* page_alloc */
/* page_free */

static unsigned long long
host_time(void){
}

static void*
host_timer_alloc(void (*fn)(void*), void* arg){
}

static int
host_timer_set_oneshot(void* timer, unsigned long delta){
}

static void
host_timer_free(void* timer){
}

/* ioremap */
/* iomem_access */

static long
host_gettid(void){
}

static void
host_jmp_buf_set(struct lkl_jmp_buf* jmpb, void (*f)(void)){
}

static void
host_jmp_buf_longjmp(struct lkl_jmp_buf* jmpb, int val){
}

static void*
host_memcpy(void* dest, const void* src, unsigned long count){
}

static void*
host_memset(void* s, int c, unsigned long count){
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
