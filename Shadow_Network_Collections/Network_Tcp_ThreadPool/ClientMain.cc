#include "Tcp_Client.hpp"

int main()
{
    TcpClientMutil client;
    client.Init();
    client.Run();
    return 0;
}