//FIMXE: Try to follow ABI..
//#include "../kernel/sched/sched.h"

#include <stddef.h>
void *memcpy(void *dest, const void *src, size_t n);

/* FIXME: Pray the frontend emits struct in this order... */

struct sched_class {
    char __dummy_method[64];
};

struct sched_class dl_sched_class;
struct sched_class rt_sched_class;
struct sched_class fair_sched_class;
struct sched_class idle_sched_class;
struct sched_class __dummy_sched_class;

extern struct sched_class dl_sched_class_temp;
extern struct sched_class rt_sched_class_temp;
extern struct sched_class fair_sched_class_temp;
extern struct sched_class idle_sched_class_temp;

void
host_lkl_initschedclasses(void){
    struct sched_class temp;
    memcpy(&dl_sched_class, &dl_sched_class_temp, sizeof(struct sched_class));
    memcpy(&rt_sched_class, &rt_sched_class_temp, sizeof(struct sched_class));
    memcpy(&fair_sched_class, &fair_sched_class_temp, sizeof(struct sched_class));
    memcpy(&idle_sched_class, &idle_sched_class_temp, sizeof(struct sched_class));
    memcpy(&__dummy_sched_class, &idle_sched_class_temp, sizeof(struct sched_class));
}

extern struct sched_class __attribute__((alias ("dl_sched_class"))) __sched_class_highest;
extern struct sched_class __attribute__((alias ("__dummy_sched_class"))) __sched_class_lowest;
