#include "Client.hpp"
#include "Server.hpp"
int main()
{
    UdpServer server;
    server.Init();
    server.Run();
    return 0;
}