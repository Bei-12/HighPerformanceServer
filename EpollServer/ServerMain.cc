#include "Server.hpp"

int main()
{
    TcpServerMutil server;
    server.Init();
    server.Start();
    return 0;
}