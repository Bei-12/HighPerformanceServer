// 目标： 主动找到服务器，然后发送数据
#include <cstring>
#include <iostream>
#include <string>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/types.h>
#include <sys/socket.h>

using namespace std;

class UdpClient
{
public:
    UdpClient(int port = 8080, string address = "127.0.0.1")
        : port_(port), address_(address)
    {
        memset((void *)&server_, 0, sizeof(server_));
        memset((void *)&peer_, 0, sizeof(peer_));
    }

    void Init()
    {
        // socket --- 接受发送
        sockfd_ = socket(AF_INET, SOCK_DGRAM, 0);
        if (sockfd_ < 0)
        {
            cout << "Socket Create Failed" << strerror(errno) << endl;
            exit(1);
        }
        server_.sin_family = AF_INET;
        server_.sin_port = htons(port_);
        server_.sin_addr.s_addr = inet_addr(address_.c_str());
    }

    void Send()
    {
        // sedto
        char buffer[4096];
        cout << "Enter Message#:";
        cin >> buffer;
        ssize_t mes = sendto(sockfd_, buffer, strlen(buffer), 0, (sockaddr *)&server_, (socklen_t)sizeof(server_));
        if (mes < 0)
        {
            cout << "Send Failed" << endl;
            return;
        }
    }

    // 接收数据
    void Receive()
    {
        char buffer[4096];
        buffer[0] = 0;
        socklen_t len = (socklen_t)sizeof(peer_);
        int mes = recvfrom(sockfd_, buffer, sizeof(buffer), 0, (struct sockaddr *)&peer_, &len);
        cout << "Message from server#:  " << buffer << endl;
    }

    void Run()
    {
        while (true)
        {
            Send();
            Receive();
        }
    }

    ~UdpClient()
    {
        close(sockfd_);
    }

private:
    int port_;
    int sockfd_;
    string address_;
    struct sockaddr_in server_;
    struct sockaddr_in peer_;
};