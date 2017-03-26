
#include <pthread.h>
#include <unistd.h>
#include <sys/syscall.h>
#include <stdio.h>

#include <string.h>
#include "../../linux/hash_linux.h"
#include "../../src/hash_performance_test.h"
#include "../hash_mod.h"

class LinuxThread : public HashThread
{
    virtual void  Start(func_thread func, HashTestData& data)
    {
        pthread_create(&handle, 0, func, &data);
    }
    virtual void  Join()
    {
        pthread_join(handle, 0);
    }
private:
    pthread_t handle;
};
static void* syscall_func_insert(void* ptr)
{
    HashTestData* test_data = (HashTestData*)ptr;
    CallContext context = {kHashInsert,0, -1};
    for(int i = test_data->start_; i < test_data->count_; ++i)
    {
        context.key = i;
        context.value = i;
        syscall(OVER_WRITE_CALL_ID, &context);
        test_data->stat.insert ++;
    }
    return 0;
}
static void* syscall_func_find(void* ptr)
{
    HashTestData* test_data = (HashTestData*)ptr;
    CallContext context = {kHashFind,0, -1};
    int temp;
    for(int i = test_data->start_; i < test_data->count_; ++i)
    {
        context.key = i;
        syscall(OVER_WRITE_CALL_ID, &context);
        if(context.value != i)
        {
            test_data->stat.lost;
        }
        test_data->stat.find ++;       
    }
    return 0;
}
static void* syscall__func_erase(void* ptr)
{
    HashTestData* test_data = (HashTestData*)ptr;
    CallContext context = {kHashErase};
    for(int i = test_data->start_; i < test_data->count_; ++i)
    {
        context.key = i;
        syscall(OVER_WRITE_CALL_ID, &context);
        test_data->stat.erase ++;
    }
    return 0;
}

int main(int argc, char** argv)
{
    
    int start_size = 100000;
    if(argc > 1) {
			  sscanf(argv[1], "%u", &start_size);
			  fprintf(stderr, "set start_size :%d\r\n", start_size);
		}
    {
        HashPerformanceTest<LinuxThread> test;
        test.set_thread_func(syscall_func_insert, syscall_func_find, syscall__func_erase);
        test.Init(1, start_size, start_size, 0);
        test.DoTest();
        test.Init(4, start_size, start_size, 0);
        test.DoTest();
    }
    return 0;
}
