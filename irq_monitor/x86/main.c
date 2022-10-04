#define _GNU_SOURCE
#include <stdio.h>
#include <string.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <fcntl.h>
#include "cacheutils.h"
#include <pthread.h>
#include <sched.h>
#include <immintrin.h>
#include <signal.h>
#include <linux/membarrier.h>

#define CORE_VICTIM 1

#define OFFSET (64 * 21)

#ifdef UMONITOR
#define INSTR_MONITOR asm volatile("umonitor %%rax" : : "a"(test + OFFSET), "c"(0), "d"(0));
#define INSTR_WAIT asm volatile("xor %%rbx, %%rbx; umwait %%rcx; jnc 1f; inc %%rbx; 1: nop" : "=b"(carry) : "a"(-1), "d"(-1), "c"(0));
#define INSTR_NAME "UMONITOR"
#endif
#ifdef MONITORX
#define INSTR_MONITOR asm volatile("monitorx" : : "a"(test + OFFSET), "c"(0), "d"(0));
#define INSTR_WAIT asm volatile("mwaitx" : : "a"(-1), "d"(-1), "c"(0));
#define INSTR_NAME "MONITORX"
#endif
#ifdef MONITOR
#define INSTR_MONITOR asm volatile("monitor" : : "a"(test + OFFSET), "c"(0), "d"(0));
#define INSTR_WAIT asm volatile("mwait" : : "a"(0), "c"(0));
#define INSTR_NAME "MONITOR"
#endif

// time interval for observing number of wakeups (in microseconds)
#define TIMER 100*1000


#define TIMER_S  (TIMER / (1000000))
#define TIMER_US (TIMER % (1000000))



struct itimerval timer = {
    .it_interval = {.tv_sec = TIMER_S, .tv_usec = TIMER_US},
    .it_value = {.tv_sec = TIMER_S, .tv_usec = TIMER_US}
};

void pin_to_core(int core) {
    cpu_set_t cpuset;
    pthread_t thread;

    thread = pthread_self();

    CPU_ZERO(&cpuset);
    CPU_SET(core, &cpuset);

    pthread_setaffinity_np(thread, sizeof(cpu_set_t), &cpuset);
}

__attribute__((aligned(4096))) char test[4096 * 256];

volatile size_t cnt = 0, run = 0, sum = 0;

void sigalarm(int num) {
    printf("Count: %zd\n", cnt);
    if(cnt > 65) {
        printf("IRQ!\n");
    }
    cnt = 0;
}

/* Application entry */
int main(int argc, char *argv[]) {
    memset(test, 1, sizeof(test));
    
    pin_to_core(CORE_VICTIM);

    signal(SIGALRM, sigalarm);
    setitimer(ITIMER_REAL, &timer, NULL);

    while(1) {
#ifdef UMONITOR
        INSTR_MONITOR
        size_t carry = 0;
        INSTR_WAIT
        cnt += carry;
#else
        INSTR_MONITOR
        INSTR_WAIT
        cnt++;
#endif
    }
    return 0;
}

