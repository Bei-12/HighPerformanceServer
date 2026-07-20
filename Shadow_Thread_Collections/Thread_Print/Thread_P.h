#include <iostream>
#include <pthread.h>
#include <unistd.h>
#include <vector>
#include <cstdlib>
#include <ctime>

using namespace std;

void *Print(void *arg);

class Thread
{
public: 
    //线程类信息需要进行补充
    Thread(int num, int worktime)
        :_num(num)
        ,_worktime(worktime)
    {}

    void Start()
    {
        // 创建线程，并让线程开始运行
        cout << "开始创建线程" << endl;
        //使用this指针，传递对象的地址
        int re = pthread_create(&_t, nullptr, Print, (void*)this);
        if(re != 0)
        {
            exit(re);
        }
    }

    void Join()
    {
        // 职责是什么？ --- 等待线程结束 回收资源
        // 为什么不能直接退出程序？ --- 资源没有回收，会导致线程资源一直没有释放，导致资源泄露
        // Join 到底在等待什么？ --- 等待前一个线程结束
        cout << "线程等待" << endl;
        int re = pthread_join(_t, nullptr);
        if(re != 0)
        {
            exit(re);
        }
        cout << "等待结束" << endl;
    }

    // pthread_t DisplayCurrentThreadID()
    // {
    //     //return pthread_self();
    // }
    pthread_t _t;
    int _num;
    int _worktime;
};

// 线程执行函数
void *Print(void *arg)
{
    Thread* t = (Thread*)arg;
    int num = (*t)._num;
    int worktime = (*t)._worktime;
    cout << "pthread_id: " << pthread_self() << endl;
    //线程启动 -> 打印开始 -> 循环工作几秒 -> 打印结束
    cout << "Thread " << num << " Start" << endl;
    cout << "Work " << worktime << " seconds..." << endl;
    sleep(worktime);
    cout << "Thread " << num << " Exit" << endl;
    return nullptr;
}
