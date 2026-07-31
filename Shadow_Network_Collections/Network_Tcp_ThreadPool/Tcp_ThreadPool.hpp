// 网络连接如何变成任务交给线程池处理
#include <iostream>
#include <queue>
#include <cstring>
#include <functional>
#include <vector>
#include <cstddef>
#include <pthread.h>
#include <unistd.h>
#include <string>
#include <arpa/inet.h>
#include <sys/types.h>
#include <sys/socket.h>

#define SIZE 4096

using namespace std;

// 任务 --- Task
class TcpTask
{
public:
    TcpTask()
        : clientfd_(-1)
    {
    }

    TcpTask(int fd)
        : clientfd_(fd)
    {
    }

    void Run()
    {
        while (true)
        {
            string s = Receive();
            if (s == "quit")
            {
                cout << "Task Run exit" << endl;
                break;
            }
            Send(s);
        }
        close(clientfd_);
    }

    ~TcpTask()
    {
        if(clientfd_ >= 0)
            close(clientfd_);
    }


private:
    string Receive()
    {
        char rec_buffer_[SIZE];
        rec_buffer_[0] = 0;
        ssize_t ret = recv(clientfd_, rec_buffer_, sizeof(rec_buffer_), 0);
        if (ret < 0)
        {
            cout << "Receive Failed" << endl;
            return "quit";
        }
        else if (ret == 0)
        {
            cout << "Client close" << endl;
            return "quit";
        }
        rec_buffer_[ret] = 0;
        if (strcmp(rec_buffer_, "quit") == 0)
        {
            cout << "bye bye" << endl;
            return "quit";
        }
        else
            cout << "Message from client#: " << rec_buffer_ << endl;
        return (string)rec_buffer_;
    }

    void Send(string s)
    {
        string send_buffer_ = "Server received ";
        send_buffer_ += s;
        ssize_t ret = send(clientfd_, send_buffer_.c_str(), send_buffer_.size() + 1, 0);
        if (ret < 0)
        {
            cout << "Send Failed" << endl;
            exit(1);
        }
        else if (ret == 0)
        {
            cout << "Server close" << endl;
            return;
        }
    }

private:
    int clientfd_;
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

    bool Push(const T &task)
    {
        // 生产任务
        // 判断任务是否空/满足
        _mutex.Lock();
        while (_qT.size() >= _capacity)
        {
            _notFull.Wait(&_mutex.lock);
        }
        _qT.push(task);
        _mutex.Unlock();
        _notEmpty.Signal(); // 唤醒消费者
        return true;        // 防止中途退出问题
    }

    bool Pop(T &task)
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
    int _capacity;       // 防止传入过多任务
    queue<TcpTask> _qT;  // 将任务存储在队列中 等待
    Mutex _mutex;        // 锁 线程执行任务时不被打扰
    Condition _notEmpty; // 队列不为空条件 --- Consumer
    Condition _notFull;  // 队列没有满条件 --- Producer
};

class Thread
{
public:
    // 线程类信息需要进行补充
    Thread(int num, BlockingQueue<TcpTask> *bq)
        : _num(num), _bq(bq)
    {
    }

    void Start()
    {
        // cout << _num << " 开始创建线程" << endl;
        int re = pthread_create(&_t, nullptr, ThreadFunction, (void *)this);
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

    static void *ThreadFunction(void *arg)
    {
        Thread *tt = (Thread *)arg;
        while (true)
        {
            TcpTask task;
            tt->_bq->Pop(task);
            task.Run();
        }
        cout << "Communicate End" << endl;
        return nullptr;
    }

    void Detach()
    {
        pthread_detach(_t);
    }

    int _num;
    Mutex *_first;
    Mutex *_second;
    BlockingQueue<TcpTask> *_bq;
private:
    int clientfd_;
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
        for (int i = 0; i < _num; i++)
        {
            _workers.push_back(new Thread(i, &_queue));
        }
    } // 指定线程个数

    void Start() // 启动线程 --- 等待任务 执行任务
    {
        for (auto &t : _workers)
        {
            t->Start();
        }
    }

    void Submit(TcpTask t) // 提交任务
    {
        // cout << "Submit task" << endl;
        _queue.Push(t);
    }

private:
    int _num; // 线程数量
    int _clientfd;
    vector<Thread *> _workers;     // 工作线程集合 --- 避免重复频繁拷贝
    BlockingQueue<TcpTask> _queue; // 任务队列
};