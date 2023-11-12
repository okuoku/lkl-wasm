#include <linux/audit.h>
#include <linux/cache.h>
#include <linux/context_tracking.h>
#include <linux/irqflags.h>
#include <linux/sched.h>
#include <linux/mm.h>
#include <linux/personality.h>
#include <linux/smp.h>
#include <linux/kernel.h>
#include <linux/signal.h>
#include <linux/errno.h>
#include <linux/wait.h>
#include <linux/ptrace.h>
#include <linux/unistd.h>
#include <linux/compiler.h>
#include <linux/syscalls.h>
#include <linux/uaccess.h>

#ifdef __wasm__

void
wasmlinux_get_signal(void* out){
    bool b;
    struct ksignal ksig;
    b = get_signal(&ksig);
    if(b){
        printk("get_signal: signal %d sizeof(ksignal) = %d sizeof(sigaction) = %d sizeof(siginfo) = %d \n", 
                ksig.sig, sizeof(ksig), sizeof(ksig.ka), sizeof(ksig.info));
        memcpy(out, &ksig, sizeof(ksig));
    }else{
        printk("get_signal: No signal.\n");
    }
}

#endif // __wasm__
