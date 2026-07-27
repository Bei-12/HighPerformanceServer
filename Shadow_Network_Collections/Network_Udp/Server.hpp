// 用Udp实现客户端和服务端互相发消息
// 创建socket 绑定自己的IP和端口 等待接收数据
// 获取发送方地址 回复信息
#include <cstring>
#include <iostream>
#include <string>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/types.h>
#include <sys/socket.h>

using namespace std;

class UdpServer
{
public:
    // 创建Socket
    UdpServer(int port = 8080, string address = "127.0.0.1")
        : port_(port), address_(address), sockfd_(-1)
    {
        memset((void *)&server_, 0, sizeof(server_));
        memset((void *)&client_, 0, sizeof(client_));
        rec_buffer_[0] = 0;
        send_buffer_[0] = 0;
    }

    void Init()
    {
        sockfd_ = socket(AF_INET, SOCK_DGRAM, 0);
        if (sockfd_ < 0)
        {
            cout << "Socket Create failed, error message:" << strerror(errno) << endl;
            exit(1);
        }

        // 绑定地址
        // 服务器告诉系统，这个socket负责哪个IP和端口的数据
        server_.sin_family = AF_INET;
        server_.sin_port = htons(port_);
        server_.sin_addr.s_addr = inet_addr(address_.c_str());
        int re = bind(sockfd_, (const struct sockaddr *)&server_, (socklen_t)sizeof(server_));
        if (re < 0)
        {
            cout << "Bind failed, error message:" << strerror(errno) << endl;
        }
    }

    // 得到client地址
    void Receive()
    {
        rec_buffer_[0] = 0;
        socklen_t len = (socklen_t)sizeof(client_);
        ssize_t ret = recvfrom(sockfd_, rec_buffer_, sizeof(rec_buffer_), 0, (sockaddr *)&client_, &len);
        if (ret < 0)
        {
            cout << "recvfrom failed " << endl;
            return;
        }
        cout << "Message from client #:" << rec_buffer_ << endl;
    }

    // 发送数据
    void Send()
    {
        send_buffer_[0] = 0;
        cout << "Enter Message#:  ";
        cin >> send_buffer_;
        ssize_t mes = sendto(sockfd_, send_buffer_, strlen(send_buffer_), 0, (struct sockaddr *)&client_, (socklen_t)sizeof(client_));
        if (mes <= 0)
        {
            cout << "Send Failed" << endl;
            return;
        }
    }

    void Run()
    {
        while (true)
        {
            Receive();
            if (strcmp(rec_buffer_, "quit") == 0)
            {
                cout << "Server Exit" << endl;
                break;
            }
            Send();
        }
    }

    ~UdpServer()
    {
        if(sockfd_ >= 0)
            close(sockfd_);
    }

private:
    int sockfd_;
    int port_;
    char send_buffer_[4096];
    char rec_buffer_[4096];
    string address_;
    struct sockaddr_in client_;
    struct sockaddr_in server_;
};