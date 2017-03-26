#ifndef __HASH_FUNCTION_H__
#define __HASH_FUNCTION_H__
// hash functions copy from fastdb
unsigned stringHashFunction(void const* ptr, size_t size);
unsigned numberHashFunction(void const* ptr, size_t size);
unsigned stringIgnoreCaseHashFunction(void const* ptr, size_t size);
unsigned wstringHashFunction(void const* ptr, size_t size);
unsigned wstringIgnoreCaseHashFunction(void const* ptr, size_t size);
#endif