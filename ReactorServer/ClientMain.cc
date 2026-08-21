#include "Client.hpp"

int main()
{
    TcpClient client;
    client.Init();
    client.Run();
    return 0;
}