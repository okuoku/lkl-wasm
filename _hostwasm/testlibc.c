#include <stdio.h>
#include <unistd.h>
#include <pthread.h>

static void*
thr_test(void* bogus){
    int counter;
    counter = 0;
    for(;;){
        fprintf(stderr, "Print from worker thread... %d\n", counter);
        counter++;
        sleep(2);
        if(counter == 2){
            return (void*)0xdeadbeef;
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

    pthread_join(thr, &p);
    printf("... Joined %p\n", p);

    for(;;){
        fprintf(stderr, "Sleep...%d\n", count);
        count++;
        sleep(1);
    }
    return 0;
}
