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
volatile char ctrl[4096];
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
	      //wait(event_internal);

        //access_speculative2(data);
        access_speculative2(data+OFFSET); // CONTROL
        cnt++;
    }
}

// Find Prime Access Patterns via tool PrimeTime (https://github.com/KULeuven-COSIC/PRIME-SCOPE/tree/main/primetime)
// B505_PR302_E0408_S1 2
void traverse_B505_PR302_E0408_S1(uint64_t* arr) {
  int i;
  for(i=0; i<12; i+=1) {
    maccess((void *) arr[i+4]);
    maccess((void *) arr[  0]);
    maccess((void *) arr[  0]);
    maccess((void *) arr[i+3]);
    maccess((void *) arr[  0]);
    maccess((void *) arr[  0]);
    maccess((void *) arr[i+2]);
    maccess((void *) arr[i+1]);
    maccess((void *) arr[i+0]);
    maccess((void *) arr[i+0]);
    maccess((void *) arr[i+1]);
    maccess((void *) arr[i+2]);
    maccess((void *) arr[i+3]);
    maccess((void *) arr[i+4]);
  }
}

void prime(uint64_t* arr)
{
  traverse_B505_PR302_E0408_S1(arr);
	traverse_B505_PR302_E0408_S1(arr);
	traverse_B505_PR302_E0408_S1(arr);
  traverse_B505_PR302_E0408_S1(arr);
	traverse_B505_PR302_E0408_S1(arr);
  traverse_B505_PR302_E0408_S1(arr);
	traverse_B505_PR302_E0408_S1(arr);
}

int main() {

  pin_to_core(CORE1);
  memset(data, 1, sizeof(data));
  //memset(ctrl, 2, sizeof(ctrl));

  int pos = 0, t = 0;
  int start = time(NULL);
  int current = time(NULL);

  Elem* eviction_set = evset_find(data);

  uint64_t evset[16];
  list_to_array(eviction_set, evset);
  size_t scope_time = 0, probe_scope_time = 0, count = 100000;

  // detect scope threshold
  for (int i = 0; i < count; i++) {
      size_t begin = rdtsc();
      maccess(evset[0]);
      size_t end = rdtsc();
      mfence();

      scope_time += end - begin;
      mfence();
  }

  for (int i = 0; i < count; i++) {
      maccess(data); mfence();

      size_t begin = rdtsc();
      maccess(evset[0]);
      size_t end = rdtsc();
      mfence();

      probe_scope_time += end - begin;
      mfence();
      prime(evset); mfence();
  }

  scope_time /= count;
  probe_scope_time /= count;

  size_t scope_threshold = (probe_scope_time + scope_time * 2) / 3;
  printf("scope_time: %ld  probe_scope_time: %ld\n", scope_time, probe_scope_time);
  printf("scope_threshold: %ld\n", scope_threshold);
  
  maccess(data); mfence();
  size_t begin = rdtsc();
  prime(evset);
  size_t end = rdtsc();
  mfence();
  printf("The spend time of per prime : %ld\n", end - begin);


  pthread_t pt;
  pthread_create(&pt, NULL, accessor, NULL);

  prime(evset); mfence();

  while(1) {

    // Windowless
    
    // Theoretical maximum value
    // cnt++;  // comment another thread to see the well-sync state.
    // maccess(data); mfence(); 

    size_t begin = rdtsc();
    maccess(evset[0]);
    size_t end = rdtsc();
    mfence();

    if (end-begin > scope_threshold) {
      pos++;
      prime(evset); mfence(); 
      //prime(evset); mfence();
    }

    // avoid a broken prime stage
    if (cnt%20==0) {
      prime(evset); mfence();
    }

    current = time(NULL);
    if(start != current) {
      start = current;
      sched_yield();

      printf("%5d / %5d (%.2f) \n", pos, cnt, pos * 100.0 / cnt);
      t++;
      pos = 0;
      cnt = 0;

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

