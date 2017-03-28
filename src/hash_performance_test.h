#pragma once
#include <vector>
#include <stdio.h>
#if defined(WINDOWS)
#include <time.h>
#else
#include <sys/time.h>
#endif
extern "C" {
#include "hash.c"
};


struct TimeInfo
{
    TimeInfo() : second_(0), microsecond_(0) {}
    long second_;
    long microsecond_; //微秒
};
class HaStopWatch
{
public:
    HaStopWatch()
    {
        is_running_ = false;
        buffer_[0] = 0;
        GeTime(start_);
    }
    void Start()
    {
        is_running_ = true;
        GeTime(start_);
        GeTime(stop_);
    }
    void Stop()
    {
        is_running_ = false;
        GeTime(stop_);
    }
    void ReStart()
    {
        Stop();
        Start();
    }
    const char* const GetPastMicrosec()
    {
        TimeInfo time = GetPastTime();
#if defined(WINDOWS)
        _snprintf(buffer_, sizeof(buffer_), "%d.%06ds", time.second_, time.microsecond_);
#else
        snprintf(buffer_, sizeof(buffer_), "%d.%06ds", time.second_, time.microsecond_);
#endif
        return buffer_;
    }
    TimeInfo GetPastTime()
    {
        TimeInfo out;
        int microsec = 0;
        int second = 0;

        if(is_running_)
        {
            TimeInfo current;
            GeTime(current);
            second = current.second_ - start_.second_;
            microsec = current.microsecond_ - start_.microsecond_;
        }

        else
        {
            second = stop_.second_ - start_.second_;
            microsec = stop_.microsecond_ - start_.microsecond_;
        }

        if(microsec < 0)
        {
            -- second;
            microsec += 1000000;
        }

        if(second >= 0)
        {
            out.second_ = second;
            out.microsecond_ = microsec;
        }

        return out;
    }
    virtual void GeTime(TimeInfo &time)
    {
#if defined(WINDOWS)
        const unsigned __int64 FILETIME_TO_TIMEVAL_SKEW = 0x19db1ded53e8000;
        SYSTEMTIME st;
        FILETIME ft;
        ULARGE_INTEGER ui;
        GetSystemTime(&st);
        SystemTimeToFileTime(&st, &ft);
        ui.LowPart = ft.dwLowDateTime;
        ui.HighPart = ft.dwHighDateTime;
        ui.QuadPart -= FILETIME_TO_TIMEVAL_SKEW;
        time.second_ = (long)(ui.QuadPart / (10000 * 1000));
        time.microsecond_ = (long)((ui.QuadPart % (10000 * 1000)) / 10);
#else
        struct timeval tv;
        gettimeofday(&tv, 0);
        time.second_ = tv. tv_sec;
        time.microsecond_ = tv.tv_usec;
#endif
    }
private:
    TimeInfo start_;
    TimeInfo stop_;
    char buffer_[24];
    bool is_running_;
} ;

struct HashTestData
{
    void Init(HashHandle handle, int start, int count)
    {
        handle_ = handle;
        start_ = start;
        end_ = start_ + count;
        count_ = count;
        memset(&stat, 0, sizeof(stat));
    }
    HashHandle handle_;
    int start_;
    int count_;
    int end_;
    
    struct Stat
    {
        int insert;
        int find;
        int erase;
        int lost;
    } stat;

};
typedef void* (*func_thread)(void*);
class HashThread
{
public:
    HashThread()
    {
    }
    virtual ~HashThread() {}

    virtual void  Start(func_thread func, HashTestData& data) = 0;
    virtual void  Join() = 0;;

};

static void* thread_func_insert(void* ptr)
{
    HashTestData* test_data = (HashTestData*)ptr;


    for(int i = test_data->start_; i < test_data->end_; ++i)
    {
        //fprintf(stderr, "insert %d\r\n", i);
        test_data->stat.insert ++;
        hash_insert(test_data->handle_, i, &i, sizeof(i), &i, sizeof(i));
    }

    return 0;
}
static void* thread_func_find(void* ptr)
{
    HashTestData* test_data = (HashTestData*)ptr;
    int temp;

    for(int i = test_data->start_; i < test_data->end_; ++i)
    {
        //fprintf(stderr, "find %d\r\n", i);
        test_data->stat.find ++;
        hash_find(test_data->handle_, i, &i, sizeof(i), &temp, sizeof(temp));

        if(temp != i)
        {
            test_data->stat.lost ++;
        }
    }

    return 0;
}
static void* thread_func_erase(void* ptr)
{
    HashTestData* test_data = (HashTestData*)ptr;

    for(int i = test_data->start_; i < test_data->end_; ++i)
    {
        //fprintf(stderr, "erase %d\r\n", i);
        test_data->stat.erase ++;
        hash_erase(test_data->handle_, i, &i, sizeof(i));
    }

    return 0;
}

// 按32位系统容量设计
template<class T> // HashThread
class HashPerformanceTest
{
public:
    HashPerformanceTest()
    {
        data_count_ = 0;
        thread_count_ = 0;
        handle_ = 0;
        insert_ = thread_func_insert;
        find_ = thread_func_find;
        erase_ = thread_func_erase;
        test_all_ = 0;
    }
    virtual ~HashPerformanceTest(void)
    {
        for(int i = 0; i < thread_count_; ++i)
        {
            delete thread_[i];
        }
    }
    void Init(int thread_count, int data_count, HashHandle handle = 0)
    {      
        data_count_ = data_count  < 8 ? 8 : data_count;
        thread_count_ = thread_count < 1 ? 1 : thread_count;
        handle_ = handle;
        int count_per_thread = data_count_ / thread_count_;
        test_datas_.clear();
        if(test_all_) {
            delete test_all_;
        }
        test_all_ = new HashTestData;
        test_all_->Init(handle_, 0, data_count_);
        for(int i = 0; i < (int)thread_.size(); ++i)
        {
            delete thread_[i];
        }

        thread_.clear();

        for(int i = 0; i < thread_count_; ++i)
        {
            HashTestData data;
            data.Init(handle_, i * count_per_thread, count_per_thread);
            test_datas_.push_back(data);
            thread_.push_back(new T());
        }
    }
    void set_thread_func(func_thread insert, func_thread find, func_thread erase)
    {
        insert_ = insert;
        find_ = find;
        erase_ = erase;
    }
    void DoTest()
    {
        if(!test_all_) {
            fprintf(stderr, "not Init();\r\n");
            return ;
        }
        HaStopWatch watch;
        watch.Start();
        int lost = 0;
        int count = 0;
        fprintf(stderr, "===============begin==============\r\n");
        
        
        thread_[0]->Start(insert_, *test_all_);
        thread_[0]->Join();

        fprintf(stderr, "insert cost: %s\r\n", watch.GetPastMicrosec());
        watch.ReStart();

        for(int i = 0; i < thread_count_; ++i)
        {
            thread_[i]->Start(find_, test_datas_[i]);
        }

        for(int i = 0; i < thread_count_; ++i)
        {
            thread_[i]->Join();
        }

        fprintf(stderr, "  find cost: %s\r\n", watch.GetPastMicrosec());
        watch.ReStart();

        thread_[0]->Start(erase_, *test_all_);
        thread_[0]->Join();

        fprintf(stderr, " erase cost: %s\r\n", watch.GetPastMicrosec());

        for(int i = 0; i < thread_count_; ++i)
        {
            HashTestData data = test_datas_[i];
            lost += data.stat.lost;
            count += data.count_;
        }

        fprintf(stderr, "stat: thread:%d, count:%d, lost:%d\r\n", thread_count_, count, lost);
        fprintf(stderr, "=============== end ==============\r\n");
    }
private:
    int data_count_;
    int thread_count_;
    HashHandle handle_;
    HashTestData* test_all_;
    std::vector<HashTestData> test_datas_;
    std::vector<HashThread*> thread_;
    func_thread insert_;
    func_thread find_;
    func_thread erase_;
};

void PrintHashInfo(HashHandle handle)
{
    fprintf(stderr, "hash name:%s\r\n"
        "size:%ld, hash_node_max_size:%ld, ability:%ld\r\n",
        hash_name(handle),
        hash_size(handle),
        hash_max_hash_node_size(handle),
        hash_ability(handle));
    
    
}
void PrintHashStat(void)
{
    fprintf(stderr, "vmalloc:%d,  vfree:%d, kmalloc:%d, kfree:%d\r\n",
        g_stat.vmalloc, g_stat.vfree,g_stat.kmalloc, g_stat.kfree);
    fprintf(stderr, "stat: m_fail:%d, i_fail:%d: i_conf:%d, ei_fail:%d\r\n",
        g_stat.malloc_failed,g_stat.insert_failed,
        g_stat.insert_conflict,g_stat.insert_failed_when_expand);
}