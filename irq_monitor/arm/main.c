#define _GNU_SOURCE
#include <stdio.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <sched.h>
#include <pthread.h>
#include <signal.h>

#define CORE_VICTIM 5

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

volatile size_t cnt = 0, run = 0, sum = 0;

void sigalarm(int num) {
    printf("Count: %zd\n", cnt);
    cnt = 0;
}

int main(int argc, char *argv[]) {
    pin_to_core(CORE_VICTIM);

    signal(SIGALRM, sigalarm);
    setitimer(ITIMER_REAL, &timer, NULL);

    while(1) {
        asm volatile("wfi");
        cnt++;
    }
    return 0;
}

