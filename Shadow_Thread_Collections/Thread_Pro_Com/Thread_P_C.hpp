// 目标： 线程通信 生产者 消费者 --- 都是线程
#include <iostream>
#include <pthread.h>
#include <queue>
#include <cstring>
#include <unistd.h>
#include <functional>

using namespace std;

// 任务 --- Task
class Task
{
public:
    // Task(int num = 1)
    //     : _num(num)
    // {
    // }
    // void Print()
    // {
    //     cout << "task: " << _num << " Start" << endl;
    // }
    // bool IsExit()
    // {
    //     return _num == -1;
    // }
    Task()
    {}

    Task(function<void()> func)
    :_func(func)
    {}

    void Run()
    {
        _func();
    }

private:
    //int _num;
    function<void()> _func;
};

class Mutex
{
private:
    char _name;

public:
    pthread_mutex_t lock;
    // 封装互斥锁
    // 初始化
    Mutex(char name = 'S')
        : _name(name)
    {
        pthread_mutex_init(&lock, nullptr);
    }

    void Lock()
    {
        int re = pthread_mutex_lock(&lock);
        if (re != 0)
            exit(re);
    }

    void Unlock()
    {
        int re = pthread_mutex_unlock(&lock);
        if (re != 0)
            exit(re);
    }

    ~Mutex()
    {
        pthread_mutex_destroy(&lock);
    }
};

class Condition // 线程同步
{
public:
    void Init()
    {
        int re = pthread_cond_init(&_cond, nullptr);
        if (re != 0)
        {
            cout << "Condition Init Error" << re << "errno message: " << strerror(re) << endl;
            exit(re);
        }
    }

    void Wait(pthread_mutex_t *m)
    {
        int re = pthread_cond_wait(&_cond, m);
        if (re != 0)
        {
            cout << "Condition Wait Error" << re << "errno message: " << strerror(re) << endl;
            exit(re);
        }
    }

    void Broadcast()
    {
        int re = pthread_cond_broadcast(&_cond);
        if (re != 0)
        {
            cout << "Condition Broadcast Error" << re << "errno message: " << strerror(re) << endl;
            exit(re);
        }
    }

    void Signal()
    {
        int re = pthread_cond_signal(&_cond);
        if (re != 0)
        {
            cout << "Condition Signal Error" << re << "errno message: " << strerror(re) << endl;
            exit(re);
        }
    }

    ~Condition()
    {
        pthread_cond_destroy(&_cond);
    }

private:
    pthread_cond_t _cond;
};

// 阻塞队列 --- Blocking Queue
class BlockingQueue
{
public:
    BlockingQueue(int capacity = 10)
        : _capacity(capacity)
    {
        _notEmpty.Init();
        _notFull.Init();
    }

    bool Push(Task &task)
    {
        // 生产任务
        // 判断任务是否空/满足
        _mutex.Lock();
        while (_qT.size() >= _capacity)
        {
            _notFull.Wait(&_mutex.lock);
        }
        _qT.push(task);
        _notEmpty.Signal(); // 唤醒消费者
        _mutex.Unlock();
        return true; // 防止中途退出问题
    }

    //Task Pop()
    bool Pop(Task &task)
    {
        // 获取任务
        _mutex.Lock();
        while (_qT.empty())
        {
            _notEmpty.Wait(&_mutex.lock);
        }
        Task t = _qT.front();
        _qT.pop();
        _notFull.Signal(); // 唤醒生产者
        _mutex.Unlock();
        return true;
    }

private:
    queue<Task> _qT;     // 将任务存储在队列中 等待
    Mutex _mutex;        // 锁 线程执行任务时不被打扰
    int _capacity;       // 防止传入过多任务
    Condition _notEmpty; // 队列不为空条件 --- Consumer
    Condition _notFull;  // 队列没有满条件 --- Producer
};

class Thread
{
public:
    // 线程类信息需要进行补充
    Thread(int num, BlockingQueue *bq /*, Mutex *first, Mutex *second*/)
        : _num(num), _bq(bq) //, _first(first), _second(second)
    //,_worktime(worktime)
    {
    }

    void Start()
    {
        cout << _num << " 开始创建线程" << endl;
        // 使用this指针，传递对象的地址
        int re = pthread_create(&_t, nullptr, ThreadRoutine, (void *)_bq);
        if (re != 0)
        {
            exit(re);
        }
    }

    void Join()
    {
        int re = pthread_join(_t, nullptr);
        if (re != 0)
        {
            exit(re);
        }
    }

    static void *ThreadRoutine(void *arg)
    {
        // // 在Consumer内部处理Task
        // cout << "Consumet start" << endl;
        // BlockingQueue *BQ = (BlockingQueue *)arg;
        // while (true)
        // {
        //     Task t = BQ->Pop();
        //     if (t.IsExit())
        //         break;
        //     t.Print();
        // }
        BlockingQueue *BQ = (BlockingQueue *)arg;
        while(true)
        {
            Task task;
            if(!BQ->Pop(task))
                break;
            task.Run();
        }
        return nullptr;
    }

    int AddCount(int num)
    {
        for (int i = 0; i < 10000; ++i)
            num++;
        return num;
    }

    int _num;
    int _worktime;
    // Counter *_counter;
    Mutex *_first;
    Mutex *_second;
    BlockingQueue *_bq;

private:
    pthread_t _t;
};
