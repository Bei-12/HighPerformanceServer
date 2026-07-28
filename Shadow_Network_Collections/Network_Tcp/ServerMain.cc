#include "TcpServer.hpp"

int main()
{
    TcpServer server;
    server.Init();
    server.Run();
    return 0;
}