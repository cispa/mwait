#ifndef EVICTION_H
#define EVICTION_H

typedef struct elem
{
    struct elem *next;
    struct elem *prev;
    int set;
    size_t delta;
    char pad[32]; // up to 64B
} Elem;

Elem* evset_find(void* addr);

#endif /* EVICTION_H */
