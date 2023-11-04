#include <stdio.h>
#include <unistd.h>

int
__original_main(int ac, char** av, char** envp){
    int count = 0;
    fprintf(stderr, "Hello, world!\n");
    for(;;){
        fprintf(stderr, "Sleep...%d\n", count);
        count++;
        sleep(1);
    }
    return 0;
}
