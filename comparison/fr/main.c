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

#define CACHE_HIT_THRESHOLD 65 
#define WINDOW_SIZE 1000

#define OFFSET (64 * 21)

volatile char __attribute__((aligned(4096))) data[4096];
size_t event_internal = 500000;

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

        wait(r);
	    //wait(event_internal);

        write_speculative2(data);
	    //write_speculative2(data+OFFSET); // CONTROL (Error Rate)
	    cnt++;
    }
}

int main() {
    memset(data, 1, sizeof(data));
    
    pin_to_core(CORE1);

    int pos = 0, t = 0; 
    int start = time(NULL);
    int current = time(NULL);

    maccess(data); flush(data); mfence();
    size_t begin = rdtsc();
    flush_reload_t(data); 
    size_t end = rdtsc();
    mfence();
    printf("F+R blind_spot: %d\n", end - begin);

    float acc_arr[100] = {0};

    size_t window_size = 3200; // Make sure the blind spot rate is less than 10%
    size_t delta = 0;
    pthread_t pt;
    pthread_create(&pt, NULL, accessor, NULL);

    while(1) {

        /* F+R */
        flush(data);
        wait(window_size);
        
        if (reload_t(data) < CACHE_HIT_THRESHOLD)
            pos++;

	    current = time(NULL);
        if(start != current) {
            start = current;
	    
            printf("window_cycle: %ld; %d / %d (%.2f)\n", wait(window_size), pos, cnt, pos * 100.0 / cnt);
	        t++;
            pos = 0;
	        cnt = 0;
            
            if (t == 1000) {
                t = 0;
                event_internal = event_internal * 4 / 5;
                printf("event internal alters!!!\n\n");
                if (event_internal < 500)
                    break;

            }

        }
    }
    
}
