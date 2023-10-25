#include <stdint.h>
static char dummy_page[64*1024];
//#define DUMMYSYM(x) const void* x = dummy_page
#define DUMMYSYM(x) extern const void* __attribute__((alias("dummy_page"))) x

/* tables */
DUMMYSYM(__setup_end);
DUMMYSYM(__setup_start);
DUMMYSYM(__start___param);
DUMMYSYM(__stop___param);
DUMMYSYM(__con_initcall_end);
DUMMYSYM(__con_initcall_start);

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

/* Additional address marks */
DUMMYSYM(__start___modver);
DUMMYSYM(__stop___modver);
DUMMYSYM(__start_pci_fixups_early);
DUMMYSYM(__end_pci_fixups_early);
DUMMYSYM(__start_pci_fixups_header);
DUMMYSYM(__end_pci_fixups_header);
DUMMYSYM(__start_pci_fixups_final);
DUMMYSYM(__end_pci_fixups_final);
DUMMYSYM(__start_pci_fixups_enable);
DUMMYSYM(__end_pci_fixups_enable);
DUMMYSYM(__start_pci_fixups_resume);
DUMMYSYM(__end_pci_fixups_resume);
DUMMYSYM(__start_pci_fixups_resume_early);
DUMMYSYM(__end_pci_fixups_resume_early);
DUMMYSYM(__start_pci_fixups_suspend);
DUMMYSYM(__end_pci_fixups_suspend);
DUMMYSYM(__start_pci_fixups_suspend_late);
DUMMYSYM(__end_pci_fixups_suspend_late);

extern char* __attribute((alias ("dummy_page"))) init_thread_union;
extern char* __attribute((alias ("dummy_page"))) init_stack;
struct threadinfo;
extern struct threadinfo init_thread_info;

int lkl_init(void* ops);
int lkl_start_kernel(const char* fmt, ...);
long lkl_syscall(long no, int nargs, long *params);

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

void host_lkl_inittbl(void); // generated
void host_lkl_initschedclasses(void);

static void
copy_init_thread_info(void){
    memcpy(dummy_page, &init_thread_info, 0x220);
}

uint32_t __attribute__((export_name ("syscall"))) 
syscall(uint32_t no, uint32_t nargs, uint32_t* in){
    return lkl_syscall(no, nargs, (long*)in);
}

void __attribute__((export_name ("init"))) 
init(void){
    copy_init_thread_info();
    host_lkl_initschedclasses();
    host_lkl_inittbl();
    lkl_init(lklhost_getops());
    lkl_start_kernel("mem=64M"); // FIXME
}
