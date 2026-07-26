#include "Client.hpp"
#include "Server.hpp"

int main()
{
    UdpClient client(8080, "127.0.0.1");
    client.Init();
    client.Run();
    return 0;
}