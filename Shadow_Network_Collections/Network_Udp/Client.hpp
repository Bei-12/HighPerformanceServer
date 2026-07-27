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
        : port_(port), address_(address), sockfd_(-1)
    {
        memset((void *)&server_, 0, sizeof(server_));
        memset((void *)&peer_, 0, sizeof(peer_));
        send_buffer_[0] = 0;
        rec_buffer_[0] = 0;
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
        // sendto
        send_buffer_[0] = 0;
        cout << "Enter Message#: ";
        cin >> send_buffer_;
        ssize_t ret = sendto(sockfd_, send_buffer_, strlen(send_buffer_), 0, (sockaddr *)&server_, (socklen_t)sizeof(server_));
        if (ret < 0)
        {
            cout << "Send Failed" << endl;
            return;
        }
    }

    // 接收数据
    void Receive()
    {
        rec_buffer_[0] = 0;
        socklen_t len = (socklen_t)sizeof(peer_);
        ssize_t ret = recvfrom(sockfd_, rec_buffer_, sizeof(rec_buffer_), 0, (struct sockaddr *)&peer_, &len);
        if (ret <= 0)
        {
            cout << "Receive Failed" << endl;
            return;
        }
        cout << "Message from server#: " << rec_buffer_ << endl;
    }

    void Run()
    {
        while (true)
        {
            Send();
            if (strcmp(send_buffer_, "quit") == 0)
            {
                cout << "Client Exit" << endl;
                break;
            }
            Receive();
        }
    }

    ~UdpClient()
    {
        if(sockfd_ >= 0)
            close(sockfd_);
    }

private:
    int port_;
    int sockfd_;
    string address_;
    char send_buffer_[4096];
    char rec_buffer_[4096];
    struct sockaddr_in server_;
    struct sockaddr_in peer_;
};