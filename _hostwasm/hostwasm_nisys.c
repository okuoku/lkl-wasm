long sys_ni_syscall(void);
#define NI(x) long x(long bogus, ...){ return sys_ni_syscall(); }

#if 0
long
sys_fadvise64_64(int a, long long b, long long c, int d){
    return sys_ni_syscall();
}
#endif

NI(sys_lookup_dcookie)
NI(sys_quotactl)
NI(sys_acct)
NI(sys_kexec_load)
NI(sys_init_module)
NI(sys_delete_module)
NI(sys_swapon)
NI(sys_swapoff)
NI(sys_mprotect)
NI(sys_msync)
NI(sys_mlock)
NI(sys_munlock)
NI(sys_mlockall)
NI(sys_munlockall)
NI(sys_mincore)
NI(sys_madvise)
NI(sys_remap_file_pages)
NI(sys_mbind)
NI(sys_get_mempolicy)
NI(sys_set_mempolicy)
NI(sys_migrate_pages)
NI(sys_move_pages)
NI(sys_perf_event_open)
NI(sys_fanotify_init)
NI(sys_fanotify_mark)
NI(sys_process_vm_readv)
NI(sys_process_vm_writev)
NI(sys_kcmp)
NI(sys_finit_module)
NI(sys_seccomp)
NI(sys_memfd_create)
NI(sys_bpf)
NI(sys_userfaultfd)
NI(sys_mlock2)
NI(sys_pkey_mprotect)
NI(sys_pkey_alloc)
NI(sys_pkey_free)
NI(sys_rseq)
NI(sys_kexec_file_load)
NI(sys_process_madvise)
NI(sys_quotactl_fd)
NI(sys_landlock_create_ruleset)
NI(sys_landlock_add_rule)
NI(sys_landlock_restrict_self)
NI(sys_set_mempolicy_home_node)

