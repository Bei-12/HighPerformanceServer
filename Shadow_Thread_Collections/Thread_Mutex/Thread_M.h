// 项目目标：创建多个线程 所有线程共同修改一个变量
// 观察数据错误 使用锁解决问题
#include <iostream>
#include <pthread.h>
#include <unistd.h>
#include <vector>
#include <cstdlib>
#include <ctime>

#define NUM 10000

using namespace std;

void *Print(void *arg);

class Mutex
{
private:
    pthread_mutex_t lock;
    char _name;

public:
    // 封装互斥锁
    // 初始化
    Mutex(char name = 'S')
    :_name(name)
    {
        pthread_mutex_init(&lock, nullptr);
    }

    void Lock()
    {
        cout << "try lock " << _name << endl;
        int re = pthread_mutex_lock(&lock);
        if (re != 0)
            exit(re);
        cout << _name << " lock success" << endl;
    }

    void Unlock()
    {
        int re = pthread_mutex_unlock(&lock);
        if (re != 0)
            exit(re);
        cout << _name << " unlock success" << endl;
    }

    ~Mutex()
    {
        pthread_mutex_destroy(&lock);
    }
};

class Counter
{
private:
    int _count;
    Mutex _mutex;

public:
    Counter(int count)
        : _count(count)
    {
    }

    void Add()
    {
        _mutex.Lock();
        for (int i = 0; i < NUM; i++)
        {
            //_mutex.Lock();
            _count++;
            //_mutex.Unlock();
        }
        _mutex.Unlock();
    }

    int Get()
    {
        return _count;
    }
};

class Thread
{
public:
    // 线程类信息需要进行补充
    Thread(int num, Mutex *first, Mutex *second)
        : _num(num), _first(first), _second(second)
    //,_worktime(worktime)
    {
    }

    void Start()
    {
        // 创建线程，并让线程开始运行
        // cout << "开始创建线程" << endl;
        // 使用this指针，传递对象的地址
        int re = pthread_create(&_t, nullptr, ThreadPoutine, (void *)this);
        if (re != 0)
        {
            exit(re);
        }
    }

    void Join()
    {
        // cout << "\n线程等待" << endl;
        int re = pthread_join(_t, nullptr);
        if (re != 0)
        {
            exit(re);
        }
        // cout << "等待结束" << endl;
    }

    static void *ThreadPoutine(void *arg)
    {
        Thread *t = (Thread *)arg;
        t->_first->Lock();
        sleep(1);
        t->_second->Lock();
        t->_second->Unlock();
        t->_first->Unlock();
        // t->_counter->Add();
        return nullptr;
    }

    pthread_t DisplayCurrentThreadID()
    {
        // return pthread_self();
    }

    int AddCount(int num)
    {
        for (int i = 0; i < 10000; ++i)
            num++;
        return num;
    }

    int _num;
    int _worktime;
    Counter *_counter;
    Mutex *_first;
    Mutex *_second;

private:
    pthread_t _t;
};

// 线程执行函数
void *Print(void *arg)
{
    Thread *t = (Thread *)arg;
    int num = (*t)._num;
    int worktime = (*t)._worktime;
    cout << "pthread_id: " << pthread_self() << endl;
    // 线程启动 -> 打印开始 -> 循环工作几秒 -> 打印结束
    cout << "Thread " << num << " Start" << endl;
    cout << "Work " << worktime << " seconds..." << endl;
    sleep(worktime);
    cout << "Thread " << num << " Exit" << endl;
    return nullptr;
}
