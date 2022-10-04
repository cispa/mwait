#define _GNU_SOURCE

#include <stdio.h>
#include <string.h>
#include <stdint.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include "cacheutils.h"
#include <pthread.h>
#include <sched.h>
#include <immintrin.h>
#include <signal.h>

// set to 0 to use the channel architecturally (faster, but lame)
#define SPECULATIVE 1
#define CORE_RECEIVER 1
#define CORE_SENDER 0

// ATTENTION: BITS_TO_SEND should be smaller than BITS_TO_RECEIVE
#define BITS_TO_SEND 108 // 100 bits + 8 for the first detected pattern
#define BITS_TO_RECEIVE 700

#define OFFSET (64 * 21)

#define WINDOW_SIZE 8

#define BYTE(x) ((x) >> 7) & 1, ((x) >> 6) & 1, ((x) >> 5) & 1, ((x) >> 4) & 1, ((x) >> 3) & 1, ((x) >> 2) & 1, ((x) >> 1) & 1, (x) & 1

volatile int running = 1;

char to_send[] = {0, 1, 0, 1, 1, 0, 0, 1}; // ASCII 'M' (in little endian)
//char to_send[] = {BYTE('T'), BYTE('e'), BYTE('s'), BYTE('t'), BYTE('!')};

void pin_to_core(int core) {
  cpu_set_t cpuset;
  pthread_t thread;

  thread = pthread_self();

  CPU_ZERO(&cpuset);
  CPU_SET(core, &cpuset);

  pthread_setaffinity_np(thread, sizeof(cpu_set_t), &cpuset);
}

uint64_t gettimestamp() {
  struct timeval tv;
  gettimeofday(&tv,NULL);
  return tv.tv_sec * (uint64_t)1000000 + tv.tv_usec;
}

#if SPECULATIVE
#define do_write write_spec
#else
#define do_write write_arch
#endif

void write_spec(void* ptr, int value) {
  speculation_start(s);
  *(char*) ptr = 3;
  *(char*) (ptr + 1) = 3;
  *(char*) (ptr + 2) = 3;
  *(char*) (ptr + 3) = 3;
  speculation_end(s);
}

void write_arch(void* ptr, int value) {
  *(volatile char*) ptr = 3;
}

__attribute__((aligned(4096))) char test[4096 * 256];

void* keystroke_thread(void* dummy) {
  pin_to_core(CORE_SENDER);
  //printf("[~] Waiting for keypress to start sending\n");
  test[OFFSET] = 1;

  //getchar();
  //printf("[+] Go! Press Ctrl+C to stop transmission\n");
  FILE* fd = fopen("send.txt", "w");
  asm volatile("mfence");
  size_t c = 0;
  int repeat = 200000;
  int zdiv = 100;

  for (int i = 0; i < BITS_TO_SEND; i++) {
    // use manchester encoding: 1 -> 01, 0 -> 10
    fprintf(fd, "%d\n", to_send[c]);
    if (to_send[c]) {
      asm volatile("mfence");
      for (int r = 0; r < repeat; r++) {
        do_write(test + OFFSET + 512, 2); // 0
      }
      asm volatile("mfence");
      for (int r = 0; r < repeat / zdiv; r++) {
        do_write(test + OFFSET, 2); // 1
      }
      asm volatile("mfence");
    } else {
      asm volatile("mfence");
      for (int r = 0; r < repeat / zdiv; r++) {
        do_write(test + OFFSET, 2); // 1
      }
      asm volatile("mfence");
      for (int r = 0; r < repeat; r++) {
        do_write(test + OFFSET + 512, 2); // 0
      }
      asm volatile("mfence");
    }
    c = (c + 1) % (sizeof(to_send) / sizeof(to_send[0]));
    asm volatile("mfence");
  }
  fflush(fd);
  printf("sender finished\n");
}

int main(int argc, char* argv[]) {
  printf("starting\n");
  pthread_t p;
  pthread_create(&p, NULL, keystroke_thread, NULL);
  sched_yield();

  pin_to_core(CORE_RECEIVER);

  memset(test, 1, sizeof(test));

  int last_val = -1;
  int ptr = 0, same = 0;

  FILE* fd = fopen("recv.txt", "w");
  uint64_t starttime = gettimestamp();
  for (size_t received = 0; received < 2 * BITS_TO_RECEIVE; received++) {
    int repeat = 8;
    int cnt = 0;
    
    for (int i = 0; i < repeat; i++) {
      asm volatile("umonitor %%rax" : : "a"(test + OFFSET), "c"(0), "d"(0));
      size_t carry = 0;
      asm volatile("xor %%rbx, %%rbx;\n\t"
                   "umwait %%rcx;\n\t"
                   "jc 1f;\n\t"  // carry is set when timeout hits ('0' is transmitted)
                   "inc %%rbx;\n\t"
                   "1: nop" : "=b"(carry) : "a"(-1), "d"(-1), "c"(0));
      cnt += carry;
    }
    cnt = cnt > (repeat / 2);

    fprintf(fd, "%d\n", cnt);
    fflush(fd);
  }
  uint64_t stoptime = gettimestamp();
  uint64_t usec_used = stoptime - starttime;
  printf("receiver finished\n");
  printf("[+] Raw Transmission rate (does not account initial overhead) %.1f b/s\n",
         1.f * BITS_TO_SEND / ((usec_used)/1000000));
  printf("[+] Raw Receiver rate (not actual bits) %.1f b/s\n",
         1.f * BITS_TO_RECEIVE / ((usec_used)/1000000));
  printf("[+] Time used: %d microseconds\n", (usec_used));

  return 0;
}

