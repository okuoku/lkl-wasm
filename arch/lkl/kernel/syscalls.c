#include <linux/stat.h>
#include <linux/irq.h>
#include <linux/sched.h>
#include <linux/interrupt.h>
#include <linux/jhash.h>
#include <linux/slab.h>
#include <linux/types.h>
#include <linux/net.h>
#include <linux/task_work.h>
#include <linux/syscalls.h>
#include <linux/kthread.h>
#include <linux/platform_device.h>
#include <asm/host_ops.h>
#include <asm/syscalls.h>
#include <asm/syscalls_32.h>
#include <asm/cpu.h>
#include <asm/sched.h>

static asmlinkage long sys_virtio_mmio_device_add(long base, long size,
						  unsigned int irq);

typedef long (*syscall_handler_t)(long arg1, ...);
#ifdef __wasm__
typedef long (*syscall_handler0_t)(void);
typedef long (*syscall_handler1_t)(long arg1);
typedef long (*syscall_handler2_t)(long arg1, long arg2);
typedef long (*syscall_handler3_t)(long arg1, long arg2, long arg3);
typedef long (*syscall_handler4_t)(long arg1, long arg2, long arg3, long arg4);
typedef long (*syscall_handler5_t)(long arg1, long arg2, long arg3, long arg4, long arg5);
typedef long (*syscall_handler6_t)(long arg1, long arg2, long arg3, long arg4, long arg5, long arg6);


/* FIXME: stub */
typedef int (*initcall_t)(void);
extern initcall_t __initcall__kmod_vhci_hcd__237_1574_vhci_hcd_init6;

static long 
callrestinit(void){
    __initcall__kmod_vhci_hcd__237_1574_vhci_hcd_init6();
    return 0;
}

#endif

#undef __SYSCALL
#define __SYSCALL(nr, sym) [nr] = (syscall_handler_t)sym,

syscall_handler_t syscall_table[__NR_syscalls] = {
	[0 ... __NR_syscalls - 1] =  (syscall_handler_t)sys_ni_syscall,
#include <asm/unistd.h>

#if __BITS_PER_LONG == 32
#include <asm/unistd_32.h>
#endif
#ifdef __wasm__
        [224 /* swapon */] = (syscall_handler_t)callrestinit
#endif
};

#ifndef __wasm__
static long run_syscall(long no, long *params)
{
	long ret;

	if (no < 0 || no >= __NR_syscalls)
		return -ENOSYS;

	ret = syscall_table[no](params[0], params[1], params[2], params[3],
				params[4], params[5]);

	task_work_run();

	return ret;
}
#else
static long run_syscall(long no, int nargs, long *params)
{
	long ret;

	if (no < 0 || no >= __NR_syscalls)
		return -ENOSYS;

        switch(nargs){
            case 0:
                ret = ((syscall_handler0_t)syscall_table[no])();
                break;
            case 1:
                ret = ((syscall_handler1_t)syscall_table[no])(params[0]);
                break;
            case 2:
                ret = ((syscall_handler2_t)syscall_table[no])(params[0], params[1]);
                break;
            case 3:
                ret = ((syscall_handler3_t)syscall_table[no])(params[0], params[1], params[2]);
                break;
            case 4:
                ret = ((syscall_handler4_t)syscall_table[no])(params[0], params[1], params[2], params[3]);
                break;
            case 5:
                ret = ((syscall_handler5_t)syscall_table[no])(params[0], params[1], params[2], params[3], params[4]);
                break;
            case 6:
                ret = ((syscall_handler6_t)syscall_table[no])(params[0], params[1], params[2], params[3], params[4], params[5]);
                break;
            default:
                return -ENOSYS;
        }

	task_work_run();

	return ret;
}
#endif


#define CLONE_FLAGS (CLONE_VM | CLONE_FS | CLONE_FILES | CLONE_THREAD |	\
		     CLONE_SIGHAND | SIGCHLD)

static int host_task_id;
static struct task_struct *host0;

static int new_host_task(struct task_struct **task)
{
	pid_t pid;

	switch_to_host_task(host0);

	pid = kernel_thread(host_task_stub, NULL, CLONE_FLAGS);
	if (pid < 0)
		return pid;

	rcu_read_lock();
	*task = find_task_by_pid_ns(pid, &init_pid_ns);
	rcu_read_unlock();

	host_task_id++;

	snprintf((*task)->comm, sizeof((*task)->comm), "host%d", host_task_id);

	return 0;
}
static void exit_task(void)
{
	do_exit(0);
}

static void del_host_task(void *arg)
{
	struct task_struct *task = (struct task_struct *)arg;
	struct thread_info *ti = task_thread_info(task);

	if (lkl_cpu_get() < 0)
		return;

	switch_to_host_task(task);
	host_task_id--;
	set_ti_thread_flag(ti, TIF_SCHED_JB);
	lkl_ops->jmp_buf_set(&ti->sched_jb, exit_task);
}

static struct lkl_tls_key *task_key;

#ifndef __wasm__
long lkl_syscall(long no, long *params)
#else
long lkl_syscall(long no, int nargs, long *params)
#endif
{
	struct task_struct *task = host0;
	long ret;

	ret = lkl_cpu_get();
	if (ret < 0)
		return ret;

	if (lkl_ops->tls_get) {
		task = lkl_ops->tls_get(task_key);
		if (!task) {
			ret = new_host_task(&task);
			if (ret)
				goto out;
			lkl_ops->tls_set(task_key, task);
		}
	}

	switch_to_host_task(task);

#ifndef __wasm__
	ret = run_syscall(no, params);
#else
	ret = run_syscall(no, nargs, params);
#endif

	if (no == __NR_reboot) {
		thread_sched_jb();
		return ret;
	}

out:
	lkl_cpu_put();

	return ret;
}

// #ifdef __wasm__ 

static long 
wasmlinux_newtask(long clone_flags){
    struct task_struct *task;
    struct task_struct *newtask;
    int ret;
    pid_t pid;

    task = lkl_ops->tls_get(task_key);

    ret = lkl_cpu_get();
    if (ret < 0)
        return 0;

    switch_to_host_task(task);

    pid = kernel_thread(host_task_stub, NULL, clone_flags);
    if (pid < 0)
        return 0;

    rcu_read_lock();
    newtask = find_task_by_pid_ns(pid, &init_pid_ns);
    rcu_read_unlock();

    host_task_id++;

    snprintf(task->comm, sizeof(task->comm), "host%d", host_task_id);

    switch_to_host_task(newtask);

    lkl_cpu_put();

    return (long)newtask;
}

long wasmlinux_create_ctx(long arg, void* ctid, void* ptid){
    struct kernel_clone_args a = {
        .flags = (arg & ~CSIGNAL),
        .exit_signal = arg & CSIGNAL,
        .fn = host_task_stub,
        .fn_arg = 0,
        .kthread = 0
    };

    struct task_struct *task;
    struct task_struct *newtask;
    int ret;
    pid_t pid;

    task = lkl_ops->tls_get(task_key);

    ret = lkl_cpu_get();
    if (ret < 0)
        return 0;

    switch_to_host_task(task);

    pid = kernel_clone(&a);
    if (pid < 0)
        return 0;

    rcu_read_lock();
    newtask = find_task_by_pid_ns(pid, &init_pid_ns);
    rcu_read_unlock();

    host_task_id++;

    snprintf(task->comm, sizeof(task->comm), "host%d", host_task_id);

    switch_to_host_task(newtask);

    lkl_cpu_put();

    return (long)newtask;
}

long wasmlinux_create_process_ctx(void){
    const long FLAGS = (CLONE_VM | SIGCHLD);

    return wasmlinux_newtask(FLAGS);
}

long wasmlinux_create_thread_ctx(void){
    const long FLAGS = (CLONE_VM | CLONE_FS | CLONE_FILES | CLONE_THREAD | CLONE_SIGHAND | SIGCHLD);

    return wasmlinux_newtask(FLAGS);
}

void wasmlinux_set_ctx(long ctx){
    lkl_ops->tls_set(task_key, (void*)ctx);
}

// #endif

static struct task_struct *idle_host_task;

/* called from idle, don't failed, don't block */
void wakeup_idle_host_task(void)
{
	if (!need_resched() && idle_host_task)
		wake_up_process(idle_host_task);
}

static int idle_host_task_loop(void *unused)
{
	struct thread_info *ti = task_thread_info(current);

	snprintf(current->comm, sizeof(current->comm), "idle_host_task");
	set_thread_flag(TIF_HOST_THREAD);
	idle_host_task = current;

	for (;;) {
		lkl_cpu_put();
		lkl_ops->sem_down(ti->sched_sem);
		if (idle_host_task == NULL) {
			lkl_ops->thread_exit();
			return 0;
		}
		schedule_tail(ti->prev_sched);
	}
}

int syscalls_init(void)
{
	snprintf(current->comm, sizeof(current->comm), "host0");
	set_thread_flag(TIF_HOST_THREAD);
	host0 = current;

	if (lkl_ops->tls_alloc) {
		task_key = lkl_ops->tls_alloc(del_host_task);
		if (!task_key)
			return -1;
	}

	if (kernel_thread(idle_host_task_loop, NULL, CLONE_FLAGS) < 0) {
		if (lkl_ops->tls_free)
			lkl_ops->tls_free(task_key);
		return -1;
	}

	return 0;
}

void syscalls_cleanup(void)
{
	if (idle_host_task) {
		struct thread_info *ti = task_thread_info(idle_host_task);

		idle_host_task = NULL;
		lkl_ops->sem_up(ti->sched_sem);
		lkl_ops->thread_join(ti->tid);
	}

	if (lkl_ops->tls_free)
		lkl_ops->tls_free(task_key);
}

SYSCALL_DEFINE3(virtio_mmio_device_add, long, base, long, size, unsigned int,
		irq)
{
	struct platform_device *pdev;
	int ret;

	struct resource res[] = {
		[0] = {
		       .start = base,
		       .end = base + size - 1,
		       .flags = IORESOURCE_MEM,
		       },
		[1] = {
		       .start = irq,
		       .end = irq,
		       .flags = IORESOURCE_IRQ,
		       },
	};

	pdev = platform_device_alloc("virtio-mmio", PLATFORM_DEVID_AUTO);
	if (!pdev) {
		dev_err(&pdev->dev, "%s: Unable to device alloc for virtio-mmio\n", __func__);
		return -ENOMEM;
	}

	ret = platform_device_add_resources(pdev, res, ARRAY_SIZE(res));
	if (ret) {
		dev_err(&pdev->dev, "%s: Unable to add resources for %s%d\n", __func__, pdev->name, pdev->id);
		goto exit_device_put;
	}

	ret = platform_device_add(pdev);
	if (ret < 0) {
		dev_err(&pdev->dev, "%s: Unable to add %s%d\n", __func__, pdev->name, pdev->id);
		goto exit_release_pdev;
	}

	return pdev->id;

exit_release_pdev:
	platform_device_del(pdev);
exit_device_put:
	platform_device_put(pdev);

	return ret;
}
