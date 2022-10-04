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

#define CORE1 2 
#define CORE2 3

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

void* accessor(void* dummy) {
    pin_to_core(CORE2);
    int seed = time(NULL); srand(seed);
    //printf("Press key to continue\n");
    //getchar();
    //printf("Go!\n");

    while(1) {
        sched_yield();
	    size_t r = rand() % event_internal + 1;

        wait(r);
        // wait(WINDOW_SIZE);

        //write_speculative2(data);
        write_speculative2(data+OFFSET); // CONTROL
        cnt++;
    }
}

int main() {
    memset(data, 1, sizeof(data));
    pin_to_core(CORE1);

    pthread_t pt;
    pthread_create(&pt, NULL, accessor, NULL);
    sched_yield();

    int pos = 0, t = 0;
    int start = time(NULL);
    int current = time(NULL);

    size_t window_size = 10;
    size_t delta = 0;

    while(1) {

        if(xbegin() == (~0u)) {
            data[0] ^= 1;
	    for (int i = 0; i < 1000; i++) asm volatile("nop\n");
            xend();
        } else {
            pos++;
        }

	    current = time(NULL);
        if(start != current) {
            start = current;

            printf("%5d / %5d (%.2f)\n", pos, cnt, pos * 100.0 / cnt);
	        t++;
            pos = 0;
            cnt = 0;

            if (t == 100) {
                event_internal = event_internal * 4 / 5;
                printf("event_internal alters!!!\n\n");
                t = 0;
                if (event_internal < 1000)
                    break;
            }

        }
    }

}

