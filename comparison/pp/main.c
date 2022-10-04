#define _GNU_SOURCE
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <fcntl.h>
#include <time.h>
#include <sched.h>
#include "eviction.h"
#include "../cacheutils.h"

#define CORE1 2 
#define CORE2 3

#define OFFSET 17*64

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

void access_speculative2(void* ptr) {
    speculation_start(s);
    *(char*)ptr;
    speculation_end(s);
}

int cnt = 0;
size_t volatile t_sync = 0;

void* accessor(void* dummy) {
    pin_to_core(CORE2);

    int seed = time(NULL); 
    srand(seed);
    printf("Go!\n");

    while(1) {
        sched_yield();
	    size_t r = rand() % event_internal + 1;

	    wait(r);

	    //access_speculative2(data);
	    access_speculative2(data+OFFSET); // CONTROL
	    cnt++;
    }
}

// Find Prime Access Patterns via tool PrimeTime (https://github.com/KULeuven-COSIC/PRIME-SCOPE/tree/main/primetime)
void traverse_B404_E1001_S4(Elem* arr) {
  Elem *ptr = arr;
  for(int i=0; i<13; i+=4) {
    maccess((void *) ptr);
    maccess((void *) ptr->next);
    maccess((void *) ptr->next->next);
    maccess((void *) arr);
    maccess((void *) ptr->next->next->next);
    ptr = ptr->next->next->next->next;
  }
}

void prime(Elem* arr)
{
    traverse_B404_E1001_S4(arr);
    //traverse_B404_E1001_S4(arr);
    //traverse_B404_E1001_S4(arr);
}

int main() {

    pin_to_core(CORE1);

    memset(data, 1, sizeof(data));

    int pos = 0, t = 0;
    int start = time(NULL);
    int current = time(NULL);

    Elem* eviction_set = evset_find(data);
    prime(eviction_set); mfence();
    
    size_t prime_time = 0, probe_prime_time = 0, count = 100000;

    // detect_prime_threshold
    for (int i = 0; i < count; i++) {
        size_t begin = rdtsc();
        prime(eviction_set);
        size_t end = rdtsc();
        mfence();

        prime_time += end - begin;
	    mfence();

	    prime(eviction_set);
        prime(eviction_set);
	    //sched_yield();
    }
    
    for (int i = 0; i < count; i++) {
	    maccess(data); mfence();

        size_t begin = rdtsc();
        prime(eviction_set);
        size_t end = rdtsc();
        mfence();

        probe_prime_time += end - begin;
	    mfence();

        // Same P+P in P+A paper (4.3), multiple probes (to recover)
        prime(eviction_set);
        prime(eviction_set);
        //sched_yield();
    }

    prime_time /= count;
    probe_prime_time /= count;

    size_t prime_threshold = (probe_prime_time + prime_time * 2) / 3;
    printf("prime_time: %ld  probe_prime_time: %ld\n", prime_time, probe_prime_time);
    printf("prime_threshold: %ld\n", prime_threshold);
    
    pthread_t pt;
    pthread_create(&pt, NULL, accessor, NULL);
    sched_yield();

    size_t blind_spot_end = 0;

    while(1) {

        //Windowless P+P : the length of evset equal to the number of LLC ways
        //wait(window_size);

        // Probe
        size_t begin = rdtsc();
        prime(eviction_set);
        size_t end = rdtsc();
	    mfence();

        if (end - begin >= prime_threshold) {
            pos++;
            // Windowless technique
            prime(eviction_set);
            prime(eviction_set);
            blind_spot_end += rdtsc() - begin;
        }

        current = time(NULL);
        if(start != current) {
            start = current;

            printf("pos / cnt: %6d / %6d (%.2f)\n", pos, cnt, pos * 100.0 / cnt);
	        t++;
            pos = 0;
            cnt = 0;
	        blind_spot_end = 0;

	        if (t == 100) {
                t = 0;
                event_internal = event_internal * 4 / 5;
                printf("event_internal alters!!!\n\n");
                if (event_internal < 1000) 
                    break;
            }
        }
    }
}

