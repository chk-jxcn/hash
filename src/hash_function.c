#include "hash_function.h"
#ifdef LINUX
#include <ctype.h>
#include <wctype.h>
#endif 

// hash functions copy from fastdb
unsigned stringHashFunction(void const* ptr, size_t size)
{
    unsigned h;
    char* key = (char*)ptr;
    int keylen = (int)size;

    for (h = 0; --keylen >= 0; h = h * 31 + *key++);

    //for (h = 0; --keylen >= 0; h = (h<<5)-h + *key++);
    return h;
}

unsigned numberHashFunction(void const* ptr, size_t size)
{
    unsigned h;
    char* key = (char*)ptr;
    int i = size - 10;
    int keylen = size;

    if(i > 0)
    {
        key += i;
        keylen -= i;
    }

    for (h = 0; --keylen >= 0; h = (h << 3) + (h << 1) + *key++);

    return h;
}

unsigned stringIgnoreCaseHashFunction(void const* ptr, size_t size)
{
    unsigned h;
    char* key = (char*)ptr;
    int keylen = (int)size;

    for (h = 0; --keylen >= 0;)
    {
        int code = *key++;
        h = h * 31 + toupper(code);
    }

    return h;
}

unsigned wstringHashFunction(void const* ptr, size_t size)
{
    unsigned h;
    wchar_t* key = (wchar_t*)ptr;
    int keylen = (int)size;

    for (h = 0; --keylen >= 0; h = h * 31 + *key++);

    return h;
}

unsigned wstringIgnoreCaseHashFunction(void const* ptr, size_t size)
{
    unsigned h;
    wchar_t* key = (wchar_t*)ptr;
    int keylen = (int)size;

    for (h = 0; --keylen >= 0;)
    {
        int code = *key++;
        h = h * 31 + towupper(code);
    }

    return h;
}