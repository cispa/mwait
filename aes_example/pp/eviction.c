#include <inttypes.h>
#include <stdlib.h>
#include <time.h>
#include <sys/mman.h>
#include <stdio.h>
#include <math.h>

#include "eviction.h"

#define EVICTION_THRESHOLD 80 
#define EVICTION_MEMORY_SIZE 64*1024*1024
#define EVICTION_SET_SIZE 16
#define PAGE_SIZE (1 << 12)

int
gt_eviction(Elem **ptr, Elem **can, char *victim);

static uint64_t rdtsc() {
  uint64_t a, d;
  asm volatile("mfence");
  asm volatile("rdtscp" : "=a"(a), "=d"(d) :: "rcx");
  a = (d << 32) | a;
  asm volatile("mfence");
  return a;
}

void memory_access(void *p) { asm volatile("movq (%0), %%rax\n" : : "c"(p) : "rax"); }

void memory_fence() { asm volatile("mfence\n"); }

int reload_time(void *ptr) {
  uint64_t start = 0, end = 0;

  start = rdtsc();
  memory_access(ptr);
  end = rdtsc();

  memory_fence();

  return (int)(end - start);
}



// initialize linked list in evset memory
// one entry per page at the correct offset!
static void
initialize_list(char *start, uint64_t size, uint64_t offset)
{
	uint64_t off = 0;
	Elem* last = NULL;
	for (off = offset; off < size - sizeof(Elem); off += PAGE_SIZE)
	{
		Elem* cur = (Elem*)(start + off);
		cur->set = -2;
		cur->delta = 0;
		cur->prev = last;
		cur->next = NULL;
		if(last){
		    last->next = cur;
		}
		last = cur;
	}
}

static Elem*
initialize_list_with_offset(Elem* reference, uint64_t offset){
    // xor mask to turn offset of reference into offset of 
    uint64_t xor = ((uint64_t)reference & (PAGE_SIZE - 1)) ^ offset;
    Elem* last = NULL;
    Elem* start = (Elem*)((uint64_t)reference ^ xor);
    while(reference){
        Elem* cur = (Elem*)((uint64_t)reference ^ xor);
        cur->set = -2;
        cur->delta = 0;
        cur->prev = last;
        cur->next = NULL;
        if(last){
            last->next = cur;
        } 
        last = cur;
        reference = reference->next;
    }
    
    return start;
}

Elem* evset_find(void* addr){

    uint64_t offset = (PAGE_SIZE - 1) & (uint64_t)addr & ~((1 << 6) - 1);
    
    Elem* start;

    // map memory
    char* memory_chunk = (char*) mmap (NULL, EVICTION_MEMORY_SIZE, PROT_READ|PROT_WRITE, MAP_PRIVATE|MAP_ANON, 0, 0);

    if(!memory_chunk){
        printf("MMAP FAILED\n");
    }

    // initialize list
    start = (Elem*)(void*)(memory_chunk + offset);
    initialize_list(memory_chunk, EVICTION_MEMORY_SIZE, offset);


    // reduce list
    Elem* can = NULL;
    int retries = 0;
    while(gt_eviction(&start, &can, addr)){
        printf("finding eviction set failed!\n");
        if(retries > 20){
            printf("max retries exceeded!\n");
            break;
        }
        // unmap memory chunk
        munmap(memory_chunk, EVICTION_MEMORY_SIZE);
        // map new memory chunk
        memory_chunk = (char*) mmap (NULL, EVICTION_MEMORY_SIZE, PROT_READ|PROT_WRITE, MAP_PRIVATE|MAP_ANON, 0, 0);
        start = (Elem*)(void*)(memory_chunk + offset);
        initialize_list(memory_chunk, EVICTION_MEMORY_SIZE, offset);
        can = NULL;
        retries ++;
    }
    if(retries < 20){
        printf("Eviction set found!\n");
    }

    // unmap memory that is no longer needed
    while(can){
        Elem* next = can->next;
        if(munmap((void*)((uint64_t)can & (~(PAGE_SIZE-1))), PAGE_SIZE)){
            printf("munmap failed!\n");
        }
        can = next;
    }
 
    // magic!
    return  start;
    
}


#define ROUNDS 50
#define MAX_BACKTRACK 50

int
list_length(Elem *ptr)
{
	int l = 0;
	while (ptr)
	{
		l = l + 1;
		ptr = ptr->next;
	}
	return l;
}

void
list_concat(Elem **ptr, Elem *chunk)
{
	Elem *tmp = (ptr) ? *ptr : NULL;
	if (!tmp)
	{
		*ptr = chunk;
		return;
	}
	while (tmp->next != NULL)
	{
		tmp = tmp->next;
	}
	tmp->next = chunk;
	if (chunk)
	{
		chunk->prev = tmp;
	}
}

void
list_split(Elem *ptr, Elem **chunks, int n)
{
	if (!ptr)
	{
		return;
	}
	int len = list_length (ptr), k = len / n, i = 0, j = 0;
	while (j < n)
	{
		i = 0;
		chunks[j] = ptr;
		if (ptr)
		{
			ptr->prev = NULL;
		}
		while (ptr != NULL && ((++i < k) || (j == n-1)))
		{
			ptr = ptr->next;
		}
		if (ptr)
		{
			ptr = ptr->next;
			if (ptr && ptr->prev) {
				ptr->prev->next = NULL;
			}
		}
		j++;
	}
}

void
list_from_chunks(Elem **ptr, Elem **chunks, int avoid, int len)
{
	int next = (avoid + 1) % len;
	if (!(*ptr) || !chunks || !chunks[next])
	{
		return;
	}
	// Disconnect avoided chunk
	Elem *tmp = chunks[avoid];
	if (tmp) {
		tmp->prev = NULL;
	}
	while (tmp && tmp->next != NULL && tmp->next != chunks[next])
	{
		tmp = tmp->next;
	}
	if (tmp)
	{
		tmp->next = NULL;
	}
	// Link rest starting from next
	tmp = *ptr = chunks[next];
	if (tmp)
	{
		tmp->prev = NULL;
	}
	while (next != avoid && chunks[next] != NULL)
	{
		next = (next + 1) % len;
		while (tmp && tmp->next != NULL && tmp->next != chunks[next])
		{
			if (tmp->next)
			{
				tmp->next->prev = tmp;
			}
			tmp = tmp->next;
		}
		if (tmp)
		{
			tmp->next = chunks[next];
		}
		if (chunks[next])
		{
			chunks[next]->prev = tmp;
		}
	}
	if (tmp)
	{
		tmp->next = NULL;
	}
}

void
shuffle(int *array, size_t n)
{
	size_t i;
	if (n > 1)
	{
		for (i = 0; i < n - 1; i++)
		{
			size_t j = i + rand() / (RAND_MAX / (n - i) + 1);
			int t = array[j];
			array[j] = array[i];
			array[i] = t;
		}
	}
}

/*
 * D012_S2 - Generated from PRIME+SCOPE/primetime
 *
 
 void traverse_D012_S2(uint64_t* arr) {
  int i;
  for(i=0; i<13; i+=2) {
    maccess((void *) arr[i+3]);
    maccess((void *) arr[i+2]);
    maccess((void *) arr[  0]);
    maccess((void *) arr[i+1]);
  }
}
 
 */

void
traverser(Elem *arg)
{
	Elem *ptr = arg;
	while (ptr && ptr->next && ptr->next->next && ptr->next->next->next)
	{
		memory_access (ptr->next->next->next);
		memory_access (ptr->next->next);
		memory_access (arg);
		memory_access (ptr->next);

		ptr = ptr->next->next;
	}
}

int
test_set(Elem *ptr, char *victim)
{
    // page walk
	memory_access (victim);
	memory_access (victim);

	memory_fence();
	traverser(ptr);
	traverser(ptr);
        
	return reload_time(victim);
}

int
tests_avg(Elem *ptr, char *victim, int rep)
{
	int i = 0, ret =0;
	unsigned int delta = 0;
	unsigned int delta_d = 0;
	for (i=0; i < rep; i++)
	{
		delta = test_set (ptr, victim);

        	// remove noise
		if (delta < 800) {
			delta_d += delta;
		}
	}
	ret = (float)delta_d / rep;
	return ret > EVICTION_THRESHOLD;
}

int
gt_eviction(Elem **ptr, Elem **can, char *victim)
{
	// Random chunk selection
	Elem **chunks = (Elem**) calloc (EVICTION_SET_SIZE + 1, sizeof (Elem*));
	if (!chunks)
	{
        printf("Could not allocate chunks!\n");
		return 1;
	}
	int *ichunks = (int*) calloc (EVICTION_SET_SIZE + 1, sizeof (int)), i;
	if (!ichunks)
	{
		printf("Could not allocate ichunks!\n");
		free (chunks);
		return 1;
	}

	int len = list_length(*ptr);
	int cans = 0;

	// Calculate length: h = log(a/(a+1), a/n)
	double sz = (double)EVICTION_SET_SIZE / len;
	double rate = (double)EVICTION_SET_SIZE / (EVICTION_SET_SIZE + 1);
	int h = ceil(log(sz) / log(rate)), l = 0;

	// Backtrack record
	Elem **back = (Elem**) calloc (h * 2, sizeof (Elem*));
	if (!back)
	{
		printf("Could not allocate back (%d, %f, %f, %d)!\n", h, sz, rate, list_length(*ptr));
		free (chunks);
		free (ichunks);
		return 1;
	}

	int repeat = 0;
	do {

		for (i=0; i < EVICTION_SET_SIZE + 1; i++)
		{
			ichunks[i] = i;
		}
		shuffle (ichunks, EVICTION_SET_SIZE + 1);

		// Reduce
		while (len > EVICTION_SET_SIZE)
		{

			list_split (*ptr, chunks, EVICTION_SET_SIZE + 1);
			int n = 0, ret = 0;

			// Try paths
			do
			{
				list_from_chunks (ptr, chunks, ichunks[n], EVICTION_SET_SIZE + 1);
				n = n + 1;
				ret = tests_avg (*ptr, victim, ROUNDS);
			}
			while (!ret && (n < EVICTION_SET_SIZE + 1));

			// If find smaller eviction set remove chunk
			if (ret && n <= EVICTION_SET_SIZE)
			{
				back[l] = chunks[ichunks[n-1]]; // store ptr to discarded chunk
				cans += list_length (back[l]); // add length of removed chunk
				len = list_length (*ptr);
				l = l + 1; // go to next lvl
			}
			// Else, re-add last removed chunk and try again
			else if (l > 0)
			{
				list_concat (ptr, chunks[ichunks[n-1]]); // recover last case
				l = l - 1;
				cans -= list_length (back[l]);
				list_concat (ptr, back[l]);
				back[l] = NULL;
				len = list_length (*ptr);
				goto mycont;
			}
			else
			{
				list_concat (ptr, chunks[ichunks[n-1]]); // recover last case
				break;
			}
		}

		break;
		mycont:
		;

	} while (l > 0 && repeat++ < MAX_BACKTRACK);

	// recover discarded elements
	for (i = 0; i < h * 2; i++)
	{
		list_concat (can, back[i]);
	}

	free (chunks);
	free (ichunks);
	free (back);

    int ret = 0;
    
    ret = tests_avg (*ptr, victim, ROUNDS);
    
    if (ret)
	{
		if (len > EVICTION_SET_SIZE)
		{
			return 1;
		}
	}
	else
	{
		return 1;
	}

	return 0;
}
