#include <sys/types.h>
#include <stdio.h>
#include <errno.h>
#include <unistd.h>
#include <pthread.h>
#include <signal.h>

static void*
thr_test(void* bogus){
    int counter;
    int r;
    counter = 0;
    for(;;){
        fprintf(stderr, "Print from worker thread... %d\n", counter);
        counter++;
        r = sleep(2);
        printf("Sleep = %d(%d)\n", r, errno);
        if(r){
            for(;;);
        }
    }
    return 0;
}

static void
sig_test_handler(int w){
    fprintf(stderr, "Handler called! %d\n", w);
}

static void
sig_test_sigaction(int w, siginfo_t* si, void* p){
    sig_test_handler(w);
}


int
__original_main(int ac, char** av, char** envp){
    pid_t pid;
    const char* next[] = {"dummy", "u", "m", 0};
    int count = 0;
    fprintf(stderr, "Hello, world!\n");
    int i,r;
    void* p;
    pthread_t thr;
    struct sigaction sa;

    /* Dump args */
    i = 0;
    while(av[i]){
        fprintf(stderr, "[%d/%d]: %s\n", i, ac, av[i]);
        i++;
    }

    i= 0;
    while(envp[i]){
        fprintf(stderr, "env [%d]: %s\n", i, envp[i]);
        i++;
    }

    /* vfork & execve test */
    if(ac >= 2 && av[1][0] == 'u'){
        fprintf(stderr, "execve-ed. my pid is %d\n", getpid());
        return 0;
    }else{
        pid = vfork();
        if(! pid){
            /* child process */
            fprintf(stderr, "Going to execve()...(I'm %d)\n", getpid());
            r = execve(av[0], next, NULL);
            fprintf(stderr, "Should not reach here %d,%d\n", r, errno);
        }else{
            /* parent process */
            fprintf(stderr, "Forked to %d (I'm %d)\n", pid, getpid());
        }
    }

    for(;;){
        fprintf(stderr, "Sleep...\n");
        sleep(1);
    }

    sigemptyset(&sa.sa_mask);

#if 0
    sa.sa_sigaction = sig_test_sigaction;
    sa.sa_flags = SA_SIGINFO;
#else
    sa.sa_handler = sig_test_handler;
    sa.sa_flags = 0;
#endif

    r = sigaction(SIGUSR1, &sa, NULL);
    fprintf(stderr, "sigaction = %d\n", r);

    raise(SIGUSR1);

    for(;;){
        fprintf(stderr, "Sleep...\n");
        sleep(1);
    }

    /*
    r = pthread_create(&thr, 0, thr_test, 0);

    for(;;){
        fprintf(stderr, "Sleep...%d\n", count);
        count++;
        sleep(1);
        if(count == 3){
            return 1;
        }
    }
    */
    return 0;
}
