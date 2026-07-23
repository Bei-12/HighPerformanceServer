// 目标：完善线程池类
#include <iostream>
#include <pthread.h>
#include <queue>
#include <cstring>
#include <unistd.h>
#include <functional>
#include <vector>
#include <cstddef>

using namespace std;

// 任务 --- Task
class Task
{
public:
    Task()
        : _exit(false), _func(nullptr)
    {
    }

    Task(function<void()> func, bool exit = false)
        : _func(func), _exit(exit)
    {
    }

    void Run()
    {
        //cout << "Task execute" << endl;
        if (_func)
            _func();
    }

    static Task Exit() // 创建退出任务
    {
        return Task(nullptr, true);
    }

    bool IsExit() // 判断是否要退出
    {
        return _exit;
    }

private:
    function<void()> _func;
    bool _exit;
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
template <class T>
class BlockingQueue
{
public:
    BlockingQueue(int capacity = 10)
        : _capacity(capacity)
    {
        _notEmpty.Init();
        _notFull.Init();
    }

    bool Push(const Task& task)
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

    bool Pop(Task& task)
    {
        // 获取任务
        _mutex.Lock();
        while (_qT.empty())
        {
            _notEmpty.Wait(&_mutex.lock);
        }
        task = _qT.front(); // 直接给任务
        _qT.pop();
        _notFull.Signal(); // 唤醒生产者
        _mutex.Unlock();
        return true;
    }

private:
    queue<T> _qT;        // 将任务存储在队列中 等待
    Mutex _mutex;        // 锁 线程执行任务时不被打扰
    int _capacity;       // 防止传入过多任务
    Condition _notEmpty; // 队列不为空条件 --- Consumer
    Condition _notFull;  // 队列没有满条件 --- Producer
};

class Thread
{
public:
    // 线程类信息需要进行补充
    Thread(int num, BlockingQueue<Task>* bq)
        : _num(num)
        ,_bq(bq)
    {
    }

    void Start()
    {
        //cout << _num << " 开始创建线程" << endl;
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
        //cout << "Thread execute" << endl;
        BlockingQueue<Task> *BQ = (BlockingQueue<Task> *)arg;
        while (true)
        {
            //cout << "Waiting task" << endl;
            Task task;
            if (!BQ->Pop(task))
            {
                //cout << "Pop failed" << endl;
                break;
            }
            //cout << "get task" << endl;
            if (task.IsExit())
            {
                //cout << "Worker exit" << endl;
                break;
            }
            task.Run();
        }
        return nullptr;
    }

    int _num;
    int _worktime;
    Mutex *_first;
    Mutex *_second;
    BlockingQueue<Task> *_bq;

private:
    pthread_t _t;
};

// 线程池思想：创建一批工作线程，让它们不断从任务队列中获取任务并执行，不是每次任务到来都创建线程
// 封装：有几个线程 怎么创建线程 怎么启动线程 怎么提交任务
class ThreadPool
{
public:
    ThreadPool(int num = 3)
        : _num(num)
    {
        for(int i =0; i < _num; i++)
        {
            _workers.push_back( new Thread(i,&_queue));
        }
    } // 指定线程个数

    void Start() // 启动线程 --- 等待任务 执行任务
    {
        for (auto& t : _workers)
        {
            t->Start();
        }
    }

    void Submit(Task t) // 提交任务
    {
        //cout << "Submit task" << endl;
        _queue.Push(t);
    }

    void Stop()
    {
        // 线程如何退出 --- 通知所有线程退出
        cout << "thread exit" << endl;
        for (int i = 0; i < _workers.size(); i++)
            _queue.Push(Task::Exit());
        for (auto &t : _workers)
            t->Join();
    }

private:
    vector<Thread*> _workers;    // 工作线程集合 --- 避免重复频繁拷贝
    BlockingQueue<Task> _queue; // 任务队列
    int _num;                   // 线程数量
    bool _running;              // 运行状态 --- 关闭线程池需要
};