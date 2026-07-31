#include "Tcp_Server.hpp"

int main()
{
    ThreadPool pool(5);
    pool.Start();
    TcpServerMutil server(&pool);
    server.Init();
    server.Start();
    return 0;
}