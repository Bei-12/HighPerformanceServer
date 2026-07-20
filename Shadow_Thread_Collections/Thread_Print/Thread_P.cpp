#include "Thread_P.h"

int main()
{
    srand(time(nullptr));
    vector<Thread> vT;
    for(int i = 1; i <= 10; i++)
    {
        int num = 1 + rand() % 3;
        Thread t(i, num);
        vT.push_back(t);
    }
    for(auto& t : vT)
    {
        t.Start();
    }
    for(auto& t : vT)
    {
        t.Join();
    }
    return 0;
}