#include "Thread_M.h"

int main()
{
    // 创建10个线程
    //  vector<Thread> vT;
    //  Counter counter(0);
    //  for(int i = 1; i <= 1000; i++)
    //  {
    //      vT.push_back(Thread(i,&counter));
    //  }

    // //启动所有线程
    // for(auto& t : vT)
    //     t.Start();

    // for(auto& t : vT)
    //     t.Join();

    // cout << "count: " << counter.Get() << endl;

    // 死锁
    Mutex A('A'), B('B');
    //Thread t1(1, &A, &B), t2(2, &B, &A);
    //死锁避免
    Thread t1(1, &A, &B), t2(2, &A, &B);
    t1.Start(), t2.Start();
    t1.Join(), t2.Join();

    return 0;
}