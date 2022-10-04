#define _GNU_SOURCE
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <fcntl.h>
#include <time.h>
#include <sched.h>
#include "../cacheutils.h"

#define CORE1 1 
#define CORE2 0

#define EVENT_INTERVAL 1000

#define OFFSET (64 * 21)

volatile char __attribute__((aligned(4096))) data[4096];
size_t event_internal = 500000;

#define INSTR_MONITOR asm volatile("umonitor %%rax" : : "a"(data), "c"(0), "d"(0));
#define INSTR_WAIT asm volatile("xor %%rbx, %%rbx; umwait %%rcx; jnc 1f; inc %%rbx; 1: nop" : "=b"(carry) : "a"(-1), "d"(-1), "c"(0));
#define INSTR_NAME "UMONITOR"

void pin_to_core(int core) {
    cpu_set_t cpuset;
    pthread_t thread;

    thread = pthread_self();

    CPU_ZERO(&cpuset);
    CPU_SET(core, &cpuset);

    pthread_setaffinity_np(thread, sizeof(cpu_set_t), &cpuset);
}

void write_speculative2(void* ptr) {
    speculation_start(s);
    *(char*)ptr ^= 1;
    speculation_end(s);
}

int cnt = 0;
size_t volatile t_sync = 0; 

void* accessor(void* dummy) {
    
    pin_to_core(CORE2);

    int seed = time(NULL); srand(seed);
    printf("Go!\n");

    while(1) {

	    sched_yield();
	    size_t r = rand() % event_internal + 1;

        // wait(r);
	    wait(EVENT_INTERVAL);

        //write_speculative2(data);
	    write_speculative2(data+OFFSET); // CONTROL
	    cnt++;
    }
}

int main() {

    memset(data, 1, sizeof(data));
    
    pin_to_core(CORE1);

    int pos = 0, t = 0; 
    int start = time(NULL);
    int current = time(NULL);

    pthread_t pt;
    pthread_create(&pt, NULL, accessor, NULL);

    while(1) {

	    /* UMONITOR */
	    INSTR_MONITOR
        size_t carry = 0;
        INSTR_WAIT
        if(!carry)
            pos++;
	
	    current = time(NULL);
        if(start != current) {
            start = current;

            printf("%5d / %5d (%.2f)\n", pos, cnt, pos * 100.0 / cnt);
            pos = 0;
            cnt = 0;
            t++;

            if (t==1000) {
                t = 0;
                event_internal = event_internal * 4 / 5;
                printf("event_internal alters!!!\n\n");
                if (event_internal < 500)
                    break;
            }

        }
    }
    
}
