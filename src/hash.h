#ifndef __HASH_H__
#define __HASH_H__
typedef unsigned int U32;
typedef unsigned short U16;
typedef unsigned long HashSize;
typedef unsigned long HashKey;

typedef enum EHashRetCode
{
    kHashError = -1,
    kHashSucc = 0,
    kHashRecordExist,
    kHashInvalidParams = 100,
    kHashInvalidOutValueLen,
    kHashInvalidExpandPercent,
    kHashMemoryError,
    kHashElementError = 200,
    kHashElementInvalidKeyLen,
    kHashElementInvalidValueLen,
    kHashArrayError = 300,
    kHashArrayInitFail,
} HashRetCode;
const char* const hash_ret_code_to_string(HashRetCode code);

typedef void* HashHandle;
// expand_percent < 50, 取默认值95， 最小值50
HashHandle hash_malloc(const char* name, HashSize start_size,  U32 expand_percent);
void hash_free(HashHandle handle);

HashRetCode hash_erase(HashHandle handle, HashKey hash_key, const void* const key, U32 key_len);
HashRetCode hash_insert(HashHandle handle, HashKey hash_key, const void* const key, U32 key_len, const void* const value, U32 value_len);
// 仅在扩容时存在线程安全问题
HashRetCode hash_find(HashHandle handle, HashKey hash_key, void* key, U32 key_len, void* const value, U32 value_len);

// 以下三个接口支持线程安全，性能有损耗
HashRetCode hash_erase_ts(HashHandle handle, HashKey hash_key, const void* const key, U32 key_len);
HashRetCode hash_insert_ts(HashHandle handle, HashKey hash_key, const void* const key, U32 key_len, const void* const value, U32 value_len);
HashRetCode hash_find_ts(HashHandle handle, HashKey hash_key, const void* const key, U32 key_len, const void* const value, U32 value_len);

typedef void* (*hash_foreach_func)(HashKey hash_key, const char* const key, U32 key_len, const char* const value, U32 value_len, void* param);
// hash_foreach_func 返回值不为0时退出， 并返回
void* hash_foreach_ts(HashHandle handle, hash_foreach_func func, void* param);

HashSize hash_max_hash_node_size(HashHandle handle);
HashSize hash_ability(HashHandle handle);
HashSize hash_size(HashHandle handle);
const char* const hash_name(HashHandle handle);
U32 hash_expand_percent(HashHandle handle);
void hash_set_expand_flag(HashHandle handle, bool disable);
#endif