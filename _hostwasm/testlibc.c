#include <stdio.h>
#include <errno.h>
#include <unistd.h>
#include <pthread.h>

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

int
__original_main(int ac, char** av, char** envp){
    int count = 0;
    fprintf(stderr, "Hello, world!\n");
    int r;
    void* p;
    pthread_t thr;
    r = pthread_create(&thr, 0, thr_test, 0);

    for(;;){
        fprintf(stderr, "Sleep...%d\n", count);
        count++;
        sleep(1);
        if(count == 3){
            return 1;
        }
    }
    return 0;
}
