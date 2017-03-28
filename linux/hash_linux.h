#ifndef __HASH_LINUX_H__
#define __HASH_LINUX_H__
#include <stdlib.h>
#include <wctype.h>
#include <string.h>
#ifndef LINUX
#define LINUX
#endif

void* vmalloc(int size)
{
    return malloc(size);
}
void vfree(void* ptr)
{
    free(ptr);
}
void* kmalloc(int size, int flag)
{
    return malloc(size);
}
void kfree(void* ptr)
{
    free(ptr);
}
typedef struct satomic_t
{
    volatile long counter;
} atomic_t;
void atomic_set(atomic_t* in, int value)
{
    __sync_val_compare_and_swap(&in->counter, in->counter, value);
}
int atomic_read(atomic_t* in)
{
    return in->counter;
}
int atomic_inc_and_test(atomic_t* in)
{
    return __sync_val_compare_and_swap(&in->counter, -1 ,0); 
}
int atomic_inc(atomic_t* in)
{
    return __sync_fetch_and_add(&in->counter, 1);
}
int atomic_dec(atomic_t* in)
{
    return  __sync_fetch_and_sub(&in->counter, 1);
}
#endif