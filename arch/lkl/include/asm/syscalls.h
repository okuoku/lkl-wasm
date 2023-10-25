#ifndef _ASM_LKL_SYSCALLS_H
#define _ASM_LKL_SYSCALLS_H

int syscalls_init(void);
void syscalls_cleanup(void);
#ifndef __wasm__
long lkl_syscall(long no, long *params);
#else
long lkl_syscall(long no, int nargs, long *params);
#endif
void wakeup_idle_host_task(void);

#define sys_mmap sys_mmap_pgoff
#define sys_mmap2 sys_mmap_pgoff
#define sys_clone sys_ni_syscall
#define sys_vfork sys_ni_syscall
#define sys_rt_sigreturn sys_ni_syscall

#include <asm-generic/syscalls.h>

#endif /* _ASM_LKL_SYSCALLS_H */
