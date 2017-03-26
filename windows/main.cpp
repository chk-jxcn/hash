extern "C" {
#include "hash_win.h"
};

#include <stdio.h>

#include "../src/hash_performance_test.h"

#include "../src/hash_function.c"

class WindowsThread : public HashThread
{
    virtual void  Start(func_thread func, HashTestData& data)
    {
        handle = CreateThread(NULL, 0, (LPTHREAD_START_ROUTINE)func, &data, 0, NULL);
    }
    virtual void  Join()
    {
        WaitForSingleObject(handle, INFINITE);
    }
private:
    HANDLE handle;
};

int main(int argn, char** argv)
{
    U32 test_count = 200000;
    U32 start_size = test_count + test_count / 8; //防止hash表自动扩容

    if(argn > 1)
    {
        sscanf(argv[1], "%ld", &test_count);
        fprintf(stderr, "set test count: %d\r\n", test_count);
        start_size = test_count + test_count / 8;

        if(argn > 2)
        {
            sscanf(argv[2], "%ld", &start_size);
            fprintf(stderr, "set start size: %d\r\n", start_size);
        }
    }

    if(0)  // key，value自定义内容及自动扩展测试
    {
        HashHandle handle = hash_malloc("test", 1000, 0);
        const char* test_data = "0123456789";
        char temp[24];
        strcpy(temp, test_data);
        char len = strlen(test_data);
        hash_insert(handle, stringHashFunction(temp, len), temp, len, temp, len);
        char result[24] = "";
        hash_find(handle, stringHashFunction(temp, len), temp, len, result, 24);
        fprintf(stderr, "find result:%s， expect:%s\r\n", result, temp);
        hash_erase(handle, stringHashFunction(temp, len), temp, len);
        len = 10;
        hash_insert(handle, 1, &len, sizeof(len), &len, sizeof(len));
        len ++;
        hash_insert(handle, 1, &len, sizeof(len), &len, sizeof(len));
        hash_find(handle, 1, &len, sizeof(len), temp, sizeof(temp));
        fprintf(stderr, "find result:%d， expect:%d\r\n", temp[0], len);
        len --;
        hash_erase(handle, 1, &len, sizeof(len));
        len ++;
        hash_find(handle, 1, &len, sizeof(len), temp, sizeof(temp));
        fprintf(stderr, "find result:%d， expect:%d\r\n", temp[0], len);

        for(int i = 0; i < 100; ++i)
        {
            hash_insert(handle, i, &i, sizeof(i), test_data, strlen(test_data));
        }

        for(int i = 80; i < 90; ++i)
        {
            hash_erase(handle, i, &i, sizeof(i));
        }

        if(hash_size(handle) != 91)
        {
            fprintf(stderr, "EXCEPTION, WRONG HASH SIZE!!!\r\n");
            system("pause");
            return 0;
        }

        PrintHashInfo(handle);
        hash_free(handle);
        PrintHashStat();
        system("pause");
        return 0;
    }

    // 性能测试
    HashHandle handle1 = hash_malloc("test", start_size, 0);

    if(!handle1)
    {
        fprintf(stderr, "hash_malloc failed\r\n");
    }

    {
        HashPerformanceTest<WindowsThread> test;
        test.Init(1, test_count , handle1);
        test.DoTest();
        test.Init(4, test_count, handle1);
        test.DoTest();
    }

    PrintHashInfo(handle1);
    hash_free(handle1);
    PrintHashStat();
    system("pause");
    return 0;
}