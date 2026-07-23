#include "Thread_P_C_L.hpp"

int main()
{
    ThreadPool pool(5);
    pool.Start();// 先启动线程，再给任务
    for(int i = 0; i < 10; ++i)
    {
        pool.Submit(Task([i](){cout << "thread " << i << " running" << endl;}));
    }
    
    sleep(5);
    pool.Stop();
    return 0;
}