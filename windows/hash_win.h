#ifndef __HASH_WIN_H__
#define __HASH_WIN_H__
#include <windows.h>
#include <process.h>

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
inline void atomic_set(atomic_t* in, int value)
{
    InterlockedExchange(&in->counter, value);
}
inline int atomic_read(atomic_t* in)
{
    return in->counter;
}
inline int atomic_inc_and_test(atomic_t* in)
{
    return InterlockedCompareExchange(&in->counter, 0, -1);
}
inline int atomic_inc(atomic_t* in)
{
    return InterlockedIncrement(&in->counter);
}
inline int atomic_dec(atomic_t* in)
{
    return  InterlockedDecrement(&in->counter);
}
#endif