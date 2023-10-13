static char dummy_page[64*1024];
#define DUMMYSYM(x) const void* x = dummy_page

/* tables */
DUMMYSYM(__initcall0_start);
DUMMYSYM(__initcall1_start);
DUMMYSYM(__initcall2_start);
DUMMYSYM(__initcall3_start);
DUMMYSYM(__initcall4_start);
DUMMYSYM(__initcall5_start);
DUMMYSYM(__initcall6_start);
DUMMYSYM(__initcall7_start);
DUMMYSYM(__initcall_end);
DUMMYSYM(__initcall_start);
DUMMYSYM(__setup_end);
DUMMYSYM(__setup_start);
DUMMYSYM(__start___param);
DUMMYSYM(__stop___param);
DUMMYSYM(__con_initcall_end);
DUMMYSYM(__con_initcall_start);
DUMMYSYM(__sched_class_highest);
DUMMYSYM(__sched_class_lowest);

/* address marks */
DUMMYSYM(__start___ex_table);
DUMMYSYM(__stop___ex_table);
DUMMYSYM(_etext);
DUMMYSYM(__init_begin);
DUMMYSYM(__init_end);
DUMMYSYM(_einittext);
DUMMYSYM(_end);
DUMMYSYM(_sinittext);
DUMMYSYM(_stext);
DUMMYSYM(__bss_start);
DUMMYSYM(__bss_stop);
DUMMYSYM(_edata);
DUMMYSYM(_sdata);
DUMMYSYM(__end_rodata);
DUMMYSYM(__start_rodata);
DUMMYSYM(__irqentry_text_end);
DUMMYSYM(__irqentry_text_start);
DUMMYSYM(__softirqentry_text_end);
DUMMYSYM(__softirqentry_text_start);

extern char* __attribute((alias ("dummy_page"))) init_thread_union;
extern char* __attribute((alias ("dummy_page"))) init_stack;

int lkl_init(void* ops);
int lkl_start_kernel(const char* fmt, ...);

typedef unsigned long size_t;

void *memset(void *s, int c, size_t n);
void *memcpy(void *dest, const void *src, size_t n);

void*
memset(void* s, int c, size_t n){
    char* p = (char*)s;
    size_t i;
    for(i=0;i!=n;i++){
        p[i] = c;
    }
    return s;
}

void*
memcpy(void* dest, const void* src, size_t n){
    char* p = (char*)dest;
    const char* q = (const char*)src;
    size_t i;
    for(i=0;i!=n;i++){
        p[i] = q[i];
    }
    return dest;
}

int lkl_printf(const char *, ...);
void lkl_bug(const char *, ...);

int lkl_printf(const char* fmt, ...){
    return 0;
}

void lkl_bug(const char* fmt, ...){
}

void* lklhost_getops(void);

void init(void){
    lkl_init(lklhost_getops());
    lkl_start_kernel("mem=64M"); // FIXME
}
