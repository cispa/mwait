#define _GNU_SOURCE
#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include <unistd.h>
#include <stdlib.h>
#include <openssl/aes.h>
#include <fcntl.h>
#include <sched.h>
#include <sys/mman.h>
#include "../cacheutils.h"
#include <pthread.h>
#include <sys/types.h>
#include <sys/stat.h>


// more encryptions show features more clearly
#define NUMBER_OF_ENCRYPTIONS (100)

#define CORE_VICTIM 1
#define CORE_ATTACKER 0

#define STEP 20
//#define STEP 2

#define T_INIT 50000
//#define T_INIT 33490

#define OFFSET 64 * 21
#define INSTR_MONITOR asm volatile("umonitor %%rax" : : "a"(test), "c"(0), "d"(0));
#define INSTR_WAIT asm volatile("xor %%rbx, %%rbx; umwait %%rcx; jnc 1f; inc %%rbx; 1: nop" : "=b"(carry) : "a"(-1), "d"(-1), "c"(0));

__attribute__((aligned(4096))) char test[4096];


void pin_to_core(int core) {
    cpu_set_t cpuset;
    pthread_t thread;

    thread = pthread_self();

    CPU_ZERO(&cpuset);
    CPU_SET(core, &cpuset);

    pthread_setaffinity_np(thread, sizeof(cpu_set_t), &cpuset);
}

typedef size_t (*sfnc_t)(void);

size_t volatile t_sync = 0;
size_t volatile t = 0;
size_t volatile fast = 0;

#define START_MEASURE     t_sync = 1; asm volatile("mfence"); 
#define STOP_MEASURE      asm volatile("mfence");

void calibration(void* unuse) {

    size_t local_t = t;
    if (fast)
        maccess(test+OFFSET);
    else
        flush(test+OFFSET);

    START_MEASURE

    for(size_t w = 0; w < local_t; w++) asm volatile("nop\nnop\nnop\nnop\nnop\nnop\nnop\nnop");

    // the runtime of the observed event
    size_t x = *(volatile size_t*)(test+OFFSET); 
    asm volatile("lfence");
    test[0] = x; 

    STOP_MEASURE
}

void* assist_thread(void* dummy) {
    pin_to_core(CORE_ATTACKER);
    sleep(1);
    printf("Measuring...\n");

    while(1) {
        calibration(NULL);
        while(t_sync);
    }
}

int main()
{
    memset(test, 1, sizeof(test));
    pin_to_core(CORE_VICTIM);

    pthread_t p;
    pthread_create(&p, NULL, assist_thread, NULL);
    sched_yield();

    t = T_INIT;
    size_t misses = 0, hits = 0, tries = 0;
    
    for (int i = 0; i < 10000; ++i) 
    {
        fast = i&1;

        for (int j = 0; j < NUMBER_OF_ENCRYPTIONS; ++j)
        {
            while(!(t_sync)); 

            INSTR_MONITOR
            size_t carry = 0;
            INSTR_WAIT

            if(carry) {
                misses++;
            } else {
                hits++;
            }
            
            tries++;
            t_sync = 0;
        }

        if(tries == NUMBER_OF_ENCRYPTIONS) {
            
            if (fast && hits <= 0.90 * NUMBER_OF_ENCRYPTIONS) {
                t -= STEP;
                printf("t: %d\n", t);
            }

            if (!fast && misses <= 0.90 * NUMBER_OF_ENCRYPTIONS) {
                t += STEP;
                printf("t: %d\n", t);
            }
            
            tries = 0;
            misses = 0;
            hits = 0;
    
        }

    }

    printf("Calibrated Padding Length: %zd!!\n",t);
    fflush(stdout);
    return 0;
}
