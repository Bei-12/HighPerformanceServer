// 阶段六目标：完善Reactor服务器
#include <signal.h>
#include <thread>

#include "TcpServer.hpp"

using namespace std;

int main()
{
    signal(SIGPIPE, SIG_IGN);
    TcpServer server;
    server.Start();

    // EventLoop loop;

    // thread t([&loop]()
    //          {
    //     cout << "EventLoop start" << endl;
    //     loop.Loop();
    //     cout << "EventLoop quit" << endl; });

    // sleep(2);

    // cout << "Call Quit" << endl;
    // loop.Quit();

    // t.join();

    return 0;
}

