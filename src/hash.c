/* Copyright (c) 2017.3.16, luozhongjie <l.zhjie@qq.com> */

#include "hash.h"

// 内部函数均不做参数检查

typedef unsigned long HashNodeIndex;
const HashNodeIndex g_invalid_index = ~(HashNodeIndex)0;
const U32 g_min_expand_percent = 50;
const U32 g_default_expand_percent = 95;

#if defined(WINDOWS) || defined(LINUX)
static const int hash_default_key_len = sizeof(long);
static const int hash_default_value_len = sizeof(long);
#else
#define hash_default_key_len 8
#define hash_default_value_len 8
#endif

// 支持递归
typedef struct SLocalLock
{
    atomic_t flag;
} MyLocalLock;
static inline void lock_init(MyLocalLock* lock)
{
    atomic_set(&lock->flag, -1);
}
static inline void lock_lock(MyLocalLock* lock)
{
    while(atomic_read(&lock->flag) >= 0 &&
            atomic_inc_and_test(&lock->flag));
}
static inline void lock_unlock(MyLocalLock* lock)
{
    atomic_set(&lock->flag, -1);
}
typedef struct SHashStatistic
{
    atomic_t vmalloc;
    atomic_t vfree;
    atomic_t kmalloc;
    atomic_t kfree;

    atomic_t insert_failed;
    atomic_t insert_failed_when_expand;
    atomic_t malloc_failed;
    atomic_t insert_conflict;
} HashStatistic;

static HashStatistic g_stat = {{0}};


void* hash_vmalloc(U32 length)
{
    void* ptr = vmalloc(length);
    atomic_inc(&g_stat.vmalloc);
    return ptr;
}
void hash_vfree(void* ptr)
{
    atomic_inc(&g_stat.vfree);
    vfree( ptr);
}
void* hash_kmalloc(U32 length)
{
    void* ptr = kmalloc(length, 0);
    atomic_inc(&g_stat.kmalloc);
    return ptr;
}
void hash_kfree(void* ptr)
{
    atomic_inc(&g_stat.kfree);
    kfree(ptr);
}

void stat_item_inc(atomic_t* item)
{
    atomic_inc(item);
}
U32 stat_item_value(atomic_t* item)
{
    return (U32)atomic_read(item);
}

typedef struct SDataContainer
{
    U32 length;
    void* value; //private
    char local[hash_default_key_len]; //private
} DataContainer;
void data_container_correct(DataContainer* handle)
{
    if(handle->length <= sizeof(handle->local))
    {
        handle->value = handle->local;
    }
}
void data_container_init(DataContainer* handle)
{
    handle->length = sizeof(handle->local);
    handle->value = (void*)handle->local;
}
void data_container_destroy(DataContainer* handle)
{
    if(handle->value && handle->value != handle->local)
    {
        hash_kfree(handle->value);
    }
}
HashRetCode data_container_set(DataContainer* handle, const void* const value, U32 len)
{
    if(len > handle->length)
    {
        void* value_new = hash_kmalloc(len);

        if(!value_new)
        {
            return kHashMemoryError;
        }

        data_container_destroy(handle);
        handle->value = value_new;
    }

    handle->length = len;

    if(value)
    {
        memcpy(handle->value, value, len);
    }

    return kHashSucc;
}
void* data_container_get(DataContainer* handle)
{
    return handle->value;
}

typedef struct SHashData
{
    HashKey hash_key;
    DataContainer key;
    DataContainer value;
} HashData;
void hash_data_init(HashData* data, const HashKey hash_key)
{
    data->hash_key = hash_key;
    data_container_init(&data->key);
    data_container_init(&data->value);
}
void hash_data_destory(HashData* data)
{
    data_container_destroy(&data->key);
    data_container_destroy(&data->value);
}
// memcpy之后 指针修复
void hash_data_correct(HashData* data)
{
    data_container_correct(&data->key);
    data_container_correct(&data->value);
}
HashRetCode hash_data_set_key(HashData* data, const void* const key, const U16 key_len)
{
    return data_container_set(&data->key, key, key_len);
}
HashRetCode hash_data_set_value(HashData* data, const void* const value, const U16 value_len)
{
    return data_container_set(&data->value, value, value_len);
}
bool hash_data_compare_key(HashData* data, const void* const key, const U16 key_len)
{
    if(data->key.length == key_len &&
            memcmp(data_container_get(&data->key), key, key_len) == 0)
    {
        return true;
    }

    return false;
}
bool hash_data_value_copy(HashData* data, void* const value_buffer, const U16 buffer_len)
{
    if(!value_buffer || buffer_len < data->value.length)
    {
        return false;
    }

    memcpy(value_buffer, data_container_get(&data->value), data->value.length);
    return true;
}
typedef struct SHashPoolNode
{
    HashNodeIndex data_node_index;
} HashPoolNode;
typedef struct SHashDataNode
{
    HashPoolNode pool_node;
    HashData data;
} HashDataNode;

// busy | free
typedef struct SHashDataNodePool
{
    HashSize ability;
    MyLocalLock lock;
    HashSize max_pop_count;
    HashSize pop_counter;
    HashDataNode* node_container;
} HashDataNodePool;
HashDataNodePool* hash_data_node_pool_malloc(HashSize count)
{
    HashNodeIndex i = 0;
    HashDataNode* cursor;
    HashSize node_size = sizeof(HashDataNode) * count;
    HashDataNodePool* handle = (HashDataNodePool*)hash_vmalloc(sizeof(HashDataNodePool) + node_size);

    if(!handle)
    {
        return 0;
    }

    lock_init(&handle->lock);
    handle->ability = count;
    handle->max_pop_count = 0;
    handle->pop_counter = 0;
    handle->node_container = (HashDataNode*)((char*)handle + sizeof(HashDataNodePool));
    cursor = handle->node_container;

    for(i = 0; i < count; ++i, ++cursor)
    {
        cursor->pool_node.data_node_index = i;
    }

    return handle;
}
HashDataNodePool* hash_data_node_pool_clone(HashSize new_count, HashDataNodePool* old)
{
    // if(new_count <= old->count) // 异常
    HashDataNodePool* new_pool = hash_data_node_pool_malloc(new_count);

    if(!new_pool)
    {
        return 0;
    }

    new_pool->ability = new_count;
    new_pool->max_pop_count = old->max_pop_count;
    new_pool->pop_counter = old->pop_counter;
    memcpy(new_pool->node_container, old->node_container, sizeof(HashDataNode) * old->ability);
    return new_pool;
}
void hash_data_node_pool_free(HashDataNodePool* handle)
{
    hash_vfree(handle);
}
// 弹出可用资源
HashDataNode* hash_data_node_pool_pop(HashDataNodePool* handle)
{
    HashNodeIndex index;
    lock_lock(&handle->lock);

    if(handle->pop_counter >= handle->ability)
    {
        lock_unlock(&handle->lock);
        return 0;
    }

    index = handle->node_container[handle->pop_counter].pool_node.data_node_index;
    handle->pop_counter++;

    if(handle->pop_counter > handle->max_pop_count)
    {
        handle->max_pop_count = handle->pop_counter;
    }

    lock_unlock(&handle->lock);
    return handle->node_container + index;
}
// 放回资源， 可能会打乱原有顺序
void hash_data_node_pool_push(HashDataNodePool* handle, HashDataNode* node)
{
    HashPoolNode* last_busy;
    HashNodeIndex temp;
    lock_lock(&handle->lock);
    --handle->pop_counter;
    last_busy = &handle->node_container[handle->pop_counter].pool_node;
    temp = last_busy->data_node_index;
    last_busy->data_node_index = node->pool_node.data_node_index ;
    node->pool_node.data_node_index = temp;
    lock_unlock(&handle->lock);
}

HashDataNode* hash_data_node_pool_get(HashDataNodePool* handle, HashNodeIndex pos)
{
    if(pos >= handle->pop_counter)
    {
        return 0;
    }

    return handle->node_container + handle->node_container[pos].pool_node.data_node_index;
}

typedef struct SHashNodeArray
{
    U16 size;
    U16 ability;
    MyLocalLock lock;
    HashDataNode* local_container[2]; // 初始化容量， 超过容量动态分配内存
    HashDataNode** hash_array_datas;
} HashNodeArray;

void hashnodearray_init(HashNodeArray* handle)
{
    handle->ability = sizeof(handle->local_container) / sizeof(handle->local_container[0]);
    handle->size = 0;
    handle->hash_array_datas = handle->local_container;
    lock_init(&handle->lock);
}
void hashnodearray_destroy(HashNodeArray* handle)
{
    lock_lock(&handle->lock);

    if(handle->local_container != handle->hash_array_datas)
    {
        hash_vfree(handle->hash_array_datas);
    }
}
HashRetCode hashnodearray_expand(HashNodeArray* handle)
{
    HashSize new_ability = handle->ability * 2;
    HashDataNode** data_old = handle->hash_array_datas;
    HashDataNode** data_new = 0;

    if(new_ability > ~(U16)0)
    {
        return kHashArrayError; // 过多冲突， 返回失败
    }

    data_new = (HashDataNode**)hash_vmalloc(new_ability * sizeof(HashDataNode*));

    if(data_new)
    {
        memcpy(data_new, data_old, sizeof(HashDataNode*)*handle->size);
        handle->hash_array_datas = data_new;
        handle->ability = (U16)new_ability;

        if(data_old != handle->local_container)
        {
            hash_vfree(data_old);
        }
    }

    else
    {
        return kHashMemoryError;
    }

    return kHashSucc;
}
U16 hashnodearray_find_index(HashNodeArray* handle, const void* const key, U32 key_len)
{
    U16 i = 0;
    HashDataNode** ptr = handle->hash_array_datas;
    HashDataNode* element_node = 0;

    for(i = 0; i < handle->size; ++i, ++ptr)
    {
        element_node = *ptr;

        if(hash_data_compare_key(&element_node->data, key, key_len) == true)
        {
            return i;
        }
    }

    return i;
}
HashDataNode* hashnodearray_find_node(HashNodeArray* handle, const void* const key, U32 key_len)
{
    U16 index = hashnodearray_find_index(handle, key, key_len);

    if(index >= handle->size)
    {
        return 0;
    }

    return handle->hash_array_datas[index];
}
HashRetCode hashnodearray_push_back(HashNodeArray* handle, HashDataNode* data)
{
    if(handle->size == handle->ability)
    {
        if(hashnodearray_expand(handle) != kHashSucc)
        {
            return kHashMemoryError;
        }
    }

    handle->hash_array_datas[handle->size] = data;
    handle->size++;
    return kHashSucc;
}
void hashnodearray_erase_by_index(HashNodeArray* handle, U16 index)
{
    handle->size--;
    handle->hash_array_datas[index] = handle->hash_array_datas[handle->size];
}
// public
HashRetCode hashnodearray_add(HashNodeArray* handle, HashDataNodePool* data_node_pool,
                              U32 hash_key, const void* const key, U32 key_len,
                              const void* const value, U32 value_len)
{
    HashDataNode* data_node = 0;
    lock_lock(&handle->lock);

    if(handle->size)
    {
        if(hashnodearray_find_node(handle, key, key_len) != 0)
        {
            lock_unlock(&handle->lock);
            atomic_inc(&g_stat.insert_conflict);
            return kHashRecordExist;
        }
    }

    data_node = hash_data_node_pool_pop(data_node_pool);

    if(data_node)
    {
        HashData* data = &data_node->data;

        if(hashnodearray_push_back(handle, data_node) != kHashSucc)
        {
            hash_data_node_pool_push(data_node_pool, data_node);
            lock_unlock(&handle->lock);
            return kHashMemoryError;
        }

        hash_data_init(data, hash_key);

        if(hash_data_set_key(data, key, key_len) != kHashSucc ||
                hash_data_set_value(data, value, value_len) != kHashSucc)
        {
            hash_data_destory(data);
            hash_data_node_pool_push(data_node_pool, data_node);
            lock_unlock(&handle->lock);
            return kHashMemoryError;
        }
    }

    else
    {
        lock_unlock(&handle->lock);
        return kHashError;
    }

    lock_unlock(&handle->lock);
    return kHashSucc;
}
void hashnodearray_erase(HashNodeArray* handle, HashDataNodePool* data_node_pool, const void* const key, U32 key_len)
{
    U16 index;;
    lock_lock(&handle->lock);
    index = hashnodearray_find_index(handle, key, key_len);

    if(index < handle->size)
    {
        HashDataNode* data_node = handle->hash_array_datas[index];
        hash_data_destory(&data_node->data);
        hash_data_node_pool_push(data_node_pool, data_node);
        hashnodearray_erase_by_index(handle, index);
    }

    lock_unlock(&handle->lock);
}
HashRetCode hashnodearray_find_and_set(HashNodeArray* handle, const void* const key, U32 key_len, void* const value, U32 value_len)
{
    HashDataNode* data_node = 0;
    lock_lock(&handle->lock);
    data_node = hashnodearray_find_node(handle, key, key_len);

    if(!data_node)
    {
        lock_unlock(&handle->lock);
        return kHashError;
    }

    if(hash_data_value_copy(&data_node->data, value, value_len) == false)
    {
        lock_unlock(&handle->lock);
        return kHashInvalidOutValueLen;
    }

    lock_unlock(&handle->lock);
    return kHashSucc;
}


typedef struct SHashNodeContainer
{
    HashSize ablility;
    HashSize max_node_array_size;
    HashNodeArray* nodes_;
} HashNodeContainer;

HashNodeContainer* hash_node_container_malloc(HashSize size)
{
    HashNodeContainer* container = 0;

    if(size > ((~(HashSize)0 - sizeof(HashNodeContainer) / sizeof(HashNodeArray))))
    {
        atomic_inc(&g_stat.malloc_failed);
        return 0;
    }

    container = (HashNodeContainer*)hash_vmalloc(sizeof(HashNodeContainer) + sizeof(HashNodeArray) * size);

    if(container)
    {
        U32 i = 0;
        HashNodeArray* node_cusor = 0;
        container->ablility = size;
        container->max_node_array_size = 0;
        container->nodes_ = (HashNodeArray*)((char*)container + sizeof(HashNodeContainer));
        node_cusor = container->nodes_;

        for(i = 0; i < size; ++i, ++node_cusor)
        {
            hashnodearray_init(node_cusor);
        }
    }

    return container;
}
void hash_node_container_free(HashNodeContainer* container)
{
    U32 i = 0;
    HashNodeArray* node_cusor = container->nodes_;

    for(i = 0; i < container->ablility; ++i, ++node_cusor)
    {
        hashnodearray_destroy(node_cusor);
    }

    hash_vfree(container);
}
HashNodeArray* hash_node_container_get_node_arry(HashNodeContainer* handle, HashKey hash_key)
{
    return &handle->nodes_[hash_key % handle->ablility];
}
HashRetCode hash_node_container_push(HashNodeContainer* handle, HashDataNode* node)
{
    HashNodeArray* node_array = hash_node_container_get_node_arry(handle, node->data.hash_key);
    return hashnodearray_push_back(node_array, node);
}

// hash_node public
HashRetCode hash_node_container_add(HashNodeContainer* handle, HashDataNodePool* data_node_pool,
                                    U32 hash_key, const void* const key, U32 key_len,
                                    const void* const value, U32 value_len)
{
    HashNodeArray* node_array = hash_node_container_get_node_arry(handle, hash_key);
    HashRetCode ret = hashnodearray_add(node_array, data_node_pool, hash_key, key, key_len, value, value_len);

    if(node_array->size > handle->max_node_array_size)
    {
        handle->max_node_array_size = node_array->size;
    }

    return ret;
}

HashRetCode hash_node_container_find(HashNodeContainer* handle, HashDataNodePool* data_node_pool,
                                     U32 hash_key, const void* key, U32 key_len,
                                     void* const value, U32 value_len)
{
    HashNodeArray* node_array = hash_node_container_get_node_arry(handle, hash_key);
    return hashnodearray_find_and_set(node_array, key, key_len, value, value_len);
}

void hash_node_container_erase(HashNodeContainer* handle, HashDataNodePool* data_node_pool,
                               U32 hash_key, const void* const key, U32 key_len)
{
    HashNodeArray* node_array = hash_node_container_get_node_arry(handle, hash_key);
    hashnodearray_erase(node_array, data_node_pool, key, key_len);
}

static const U32 g_hash_fingerprint = 0x12345678;
typedef struct SMyHash
{
    volatile U32 fingerprint;
    char name[48];
    MyLocalLock expand_lock;
    atomic_t call_counter;
    atomic_t stop_the_world;
    bool expand_disabled; // 内存申请失败时赋值true
    U32 expand_when_percent; //
    HashSize expand_when_size; //
    HashDataNodePool* data_node_pool ;
    HashNodeContainer* node_container;
} MyHash;

//  HashNodeContainer* node_container
//  -----------------
// | HashNodeArray 1 | -> HashNodeArray<HashDataNode*>
//  -----------------
// | HashNodeArray . |
//  -----------------
// | HashNodeArray n |
//  -----------------


// 外部接口调用
#define HASH_CHECK_RET(hash, ret) if(!hash || hash->fingerprint!=g_hash_fingerprint){return ret;}

HashSize hash_get_size(MyHash* handle)
{
    return handle->data_node_pool->pop_counter;
}
void hash_set_expand_percent(MyHash* handle, U32 percent)
{
    if(percent < g_min_expand_percent)
    {
        percent = g_default_expand_percent;
    }

    handle->expand_when_percent = percent;
    handle->expand_when_size = handle->data_node_pool->ability * percent / 100;
}

//TODO  hash_expand失败原因为内存不足， 需要设置重试间隔
void hash_expand(MyHash* hash)
{
    atomic_inc(&hash->stop_the_world);
    lock_lock(&hash->expand_lock);

    if(hash->expand_disabled == true) // hash_expand失败后赋值true
    {
        goto exit;
    }

    if(hash_get_size(hash)  >= hash->expand_when_size)
    {
        HashSize new_size = hash->data_node_pool->ability * 2;
        HashDataNodePool* data_node_pool_new;
        HashNodeContainer* hash_node_container = 0;

        while(atomic_read(&hash->call_counter)); // 等待所有调用退出

        data_node_pool_new = hash_data_node_pool_clone(new_size, hash->data_node_pool);

        if(!data_node_pool_new)
        {
            stat_item_inc(&g_stat.malloc_failed);
            hash->expand_disabled = true;
            goto exit;
        }

        hash_data_node_pool_free(hash->data_node_pool);
        hash->data_node_pool = data_node_pool_new;
        hash->expand_when_size *= 2;
        hash_node_container = hash_node_container_malloc(new_size);

        if(hash_node_container)
        {
            HashDataNode* data_node = 0;
            HashNodeIndex i = 0;
            hash_node_container_free(hash->node_container);

            while(data_node = hash_data_node_pool_get(hash->data_node_pool, i++), data_node)
            {
                hash_data_correct(&data_node->data); //hasharray_expand, memcpy后 异常数据修复
                hash_node_container_push(hash_node_container, data_node);
            }
        }

        else
        {
            // 导致哈希桶冲突
            hash->expand_disabled = true;
        }
    }

exit:
    lock_unlock(&hash->expand_lock);
    atomic_dec(&hash->stop_the_world);
}

// public method
void hash_free(HashHandle handle)
{
    MyHash* hash = (MyHash*)handle;
    HashDataNode* data_node = 0;
    HashNodeIndex i = 0;
    HASH_CHECK_RET(hash, );
    lock_lock(&hash->expand_lock);

    if(hash->fingerprint == 0)
    {
        return ;
    }

    hash->fingerprint = 0; // 句柄置为无效

    while(atomic_read(&hash->stop_the_world));

    while(atomic_read(&hash->call_counter)); // 等待正在进行的调用终止

    if(hash->node_container)
    {
        hash_node_container_free(hash->node_container);
    }

    while(data_node = hash_data_node_pool_get(hash->data_node_pool, i++), data_node)
    {
        hash_data_destory(&data_node->data);
    }

    hash_data_node_pool_free(hash->data_node_pool);
    hash_kfree(hash);
}

HashHandle hash_malloc(const char* name, HashSize start_size, U32 expand_percent)
{
    MyHash* hash = (MyHash*)hash_kmalloc(sizeof(MyHash));

    if(!hash)
    {
        return 0;
    }

    hash->fingerprint = g_hash_fingerprint;
    strncpy(hash->name, name, sizeof(hash->name));
    start_size = start_size < 8 ? 8 : start_size; // 最小值取8
    atomic_set(&hash->call_counter, 0);
    atomic_set(&hash->stop_the_world, 0);
    lock_init(&hash->expand_lock);
    hash->node_container = 0;
    hash->expand_disabled = false;
    hash->data_node_pool = hash_data_node_pool_malloc(start_size);

    if(!hash->data_node_pool)
    {
        hash_free(hash);
        return 0;
    }

    hash->node_container = hash_node_container_malloc(start_size);

    if(!hash->node_container)
    {
        hash_data_node_pool_free(hash->data_node_pool);
        hash_free(hash);
        return 0;
    }

    hash_set_expand_percent(hash, expand_percent);
    return hash;
}

HashRetCode hash_erase(HashHandle handle, HashKey hash_key, const void* const key, U32 key_len)
{
    MyHash* hash = (MyHash*)handle;

    if(!key || !key_len)
    {
        return kHashInvalidParams;
    }

    HASH_CHECK_RET(hash, kHashSucc);

    while(atomic_read(&hash->stop_the_world));

    atomic_inc(&hash->call_counter);
    hash_node_container_erase(hash->node_container, hash->data_node_pool, hash_key, key, key_len);
    atomic_dec(&hash->call_counter);
    return kHashSucc;
}
HashRetCode hash_insert(HashHandle handle, HashKey hash_key, const void* const key, U32 key_len, const void* const value, U32 value_len)
{
    HashRetCode ret = kHashSucc;
    MyHash* hash = (MyHash*)handle;
    HASH_CHECK_RET(hash, kHashError);

    if(!key || !key_len || !value || !value_len)
    {
        return kHashInvalidParams;
    }

    if(hash->data_node_pool->pop_counter >= hash->expand_when_size)
    {
        hash_expand(hash);
    }

    atomic_inc(&hash->call_counter);
    ret = hash_node_container_add(hash->node_container, hash->data_node_pool, hash_key, key, key_len, value, value_len);

    if(ret != kHashSucc)
    {
        stat_item_inc(&g_stat.insert_failed);
    }

    atomic_dec(&hash->call_counter);
    return ret;
}
HashRetCode hash_find(HashHandle handle, HashKey hash_key, void* key, U32 key_len, void* const value, U32 value_len)
{
    HashRetCode ret = kHashSucc;
    MyHash* hash = (MyHash*)handle;

    if(!key || !key_len)
    {
        return kHashInvalidParams;
    }

    HASH_CHECK_RET(hash, kHashError);

    while(atomic_read(&hash->stop_the_world));

    atomic_inc(&hash->call_counter);
    ret = hash_node_container_find(hash->node_container, hash->data_node_pool, hash_key, key, key_len, value, value_len);
    atomic_dec(&hash->call_counter);
    return ret;
}
void hash_set_expand_flag(HashHandle handle, bool disable)
{
    MyHash* hash = (MyHash*)handle;
    HASH_CHECK_RET(hash, );
    atomic_inc(&hash->call_counter);
    lock_lock(&hash->expand_lock);
    hash->expand_disabled = disable;
    lock_unlock(&hash->expand_lock);
    atomic_dec(&hash->call_counter);
}
HashSize hash_max_hash_node_size(HashHandle handle)
{
    MyHash* hash = (MyHash*)handle;
    HASH_CHECK_RET(hash, 0);
    return hash->node_container->max_node_array_size;
}
HashSize hash_ability(HashHandle handle)
{
    MyHash* hash = (MyHash*)handle;
    HASH_CHECK_RET(hash, 0);
    return hash->data_node_pool->ability;
}
HashSize hash_size(HashHandle handle)
{
    MyHash* hash = (MyHash*)handle;
    HASH_CHECK_RET(hash, 0);
    return hash_get_size(hash);
}
const char* const hash_name(HashHandle handle)
{
    MyHash* hash = (MyHash*)handle;
    HASH_CHECK_RET(hash, "");
    return hash->name;
}
U32 hash_expand_percent(HashHandle handle)
{
    MyHash* hash = (MyHash*)handle;
    HASH_CHECK_RET(hash, 0);
    return hash->expand_when_percent;
}

#define HASH_CASE_RETURN(code) case code: return #code
const char* const hash_ret_code_to_string(HashRetCode code)
{
    // UE replace
    //%[ ]++^([a-zA-Z]+^)*$
    //    HASH_CASE_RETURN(^1);
    switch(code)
    {
        HASH_CASE_RETURN(kHashError);
        HASH_CASE_RETURN(kHashSucc);
        HASH_CASE_RETURN(kHashRecordExist);
        HASH_CASE_RETURN(kHashInvalidParams);
        HASH_CASE_RETURN(kHashInvalidOutValueLen);
        HASH_CASE_RETURN(kHashInvalidExpandPercent);
        HASH_CASE_RETURN(kHashMemoryError);
        HASH_CASE_RETURN(kHashElementError);
        HASH_CASE_RETURN(kHashElementInvalidKeyLen);
        HASH_CASE_RETURN(kHashElementInvalidValueLen);
        HASH_CASE_RETURN(kHashArrayError);
        HASH_CASE_RETURN(kHashArrayInitFail);

    default :
        return "unknown error";
    }
}