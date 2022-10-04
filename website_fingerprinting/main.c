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
#include <signal.h>
#include <linux/membarrier.h>

#include "core_observer.h"
#define CORE_WEBSITE 1

#define OFFSET (64 * 21)

#ifdef UMONITOR
#define INSTR_MONITOR asm volatile("umonitor %%rax" : : "a"(test + OFFSET), "c"(0), "d"(0));
#define INSTR_WAIT asm volatile("umwait %%rcx" : : "a"(-1), "d"(-1), "c"(0));
#define INSTR_NAME "UMONITOR"
#endif
#ifdef MONITORX
#define INSTR_MONITOR asm volatile("monitorx" : : "a"(test + OFFSET), "c"(0), "d"(0));
#define INSTR_WAIT asm volatile("mwaitx" : : "a"(-1), "d"(-1), "c"(0));
#define INSTR_NAME "MONITORX"
#endif
#ifdef WFI
#define INSTR_MONITOR 
#define INSTR_WAIT asm volatile("wfi");
#define INSTR_NAME "WFI"
#endif

// time interval for observing number of wakeups (in microseconds)
#define TIMER 10*1000


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
volatile FILE* f;

void sigalarm(int num) {
    printf("Count: %zd\n", cnt);
    fprintf(f, "%zd\n", cnt);
    fflush(f);
    cnt = 0;
}

void* open_ws(void* ws) {
    usleep(100000);
    char cmd[128];
    sprintf(cmd, "curl %s", (char*)ws);
    system(cmd);
    usleep(100000);
    exit(0);
}

/* Application entry */
int main(int argc, char *argv[]) {
    if(argc <= 1) {
        printf("Usage: %s <website>\n", argv[0]);
        exit(1);
    }
    memset(test, 1, sizeof(test));
    
    pin_to_core(CORE_OBSERVER);
    
    f = fopen("log.txt", "w");

    signal(SIGALRM, sigalarm);
    setitimer(ITIMER_REAL, &timer, NULL);
    
    pthread_t p;
    pthread_create(&p, NULL, open_ws, argv[1]);

    while(1) {
        INSTR_MONITOR
        INSTR_WAIT
        cnt++;
    }

    return 0;
}

