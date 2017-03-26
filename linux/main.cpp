

extern "C" {
#include "hash_linux.h"
#include <stdio.h>
#include <pthread.h>
#include <sys/syscall.h>
};

#include <stdio.h>

#include "../src/hash_performance_test.h"

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

int main(int argn, char** argv)
{
    U32 test_count = 10000;
    U32 start_size = test_count + test_count/8; //·ÀÖ¹hash±í×Ô¶¯À©ÈÝ
    if(argn > 1)
    {
        sscanf(argv[1], "%ld", &test_count);
        fprintf(stderr, "set test count: %d\r\n", test_count);
        start_size = test_count + test_count/8;
        if(argn > 2)
        {
            sscanf(argv[2], "%ld", &start_size);
            fprintf(stderr, "set start size: %d\r\n", start_size);
        }
    }
    HashHandle handle = hash_malloc("test", start_size, 0);
    if(!handle) {
        fprintf(stderr, "hash_malloc failed\r\n");
    }
    {
        HashPerformanceTest<LinuxThread> test;
        test.Init(1, test_count, handle);
        test.DoTest();
        test.Init(4, test_count, handle);
        test.DoTest();
    }

    PrintHashInfo(handle);
    hash_free(handle);
    PrintHashStat();
    return 0;
}
