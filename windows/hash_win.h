#ifndef __HASH_WIN_H__
#define __HASH_WIN_H__
#include <windows.h>
#include <process.h>
#ifndef WINDOWS
#define WINDOWS
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
#endif