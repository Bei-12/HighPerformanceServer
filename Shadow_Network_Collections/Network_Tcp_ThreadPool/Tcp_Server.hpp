// 一个服务器服务多个客户端
// socket bind listen accept recv send
#include <iostream>
#include <cstring>
#include <string>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/types.h>
#include <sys/socket.h>

#include "Tcp_ThreadPool.hpp"

// 服务器包括线程

using namespace std;

#define Port 8080
#define Address "127.0.0.1"
#define SIZE 4096

class TcpServerMutil
{
public:
    TcpServerMutil(ThreadPool* pool)
        : sockfd_(-1), port_(Port ), address_(Address), pool_(pool)
    {
        memset((void *)&server_, 0, sizeof(server_));
    }

    void Init()
    {
        // socket bind listen
        sockfd_ = socket(AF_INET, SOCK_STREAM, 0);
        if (sockfd_ < 0)
        {
            cout << "Socket Created Failed" << errno << "errno Message:" << strerror(errno) << endl;
            exit(1);
        }

        server_.sin_family = AF_INET;
        server_.sin_port = htons(port_);
        server_.sin_addr.s_addr = inet_addr(address_.c_str());
        int re = bind(sockfd_, (struct sockaddr *)&server_, (socklen_t)sizeof(server_));
        if (re < 0)
        {
            cout << "Bind Failed, errno message: " << strerror(errno) << endl;
            exit(1);
        }
     
        re = listen(sockfd_, 5); 
        if (re < 0)
        {
            cout << "Bind Failed, errno message: " << strerror(errno) << endl;
            exit(1);
        }
    }

    // Accept() -> 生成任务 -> push到线程池
    int Accept()
    {
        sockaddr_in client_;
        int clientfd_;
        memset((void*)&client_, 0, sizeof(client_));
        client_.sin_family = AF_INET;
        socklen_t len = (socklen_t)sizeof(client_);
        clientfd_ = accept(sockfd_, (struct sockaddr *)&client_, &len);
        if (clientfd_ < 0)
        {
            cout << "Accept Failed" << errno << "errno Message:" << strerror(errno) << endl;
            exit(1);
        }
        cout << "Client ip: " << inet_ntoa(client_.sin_addr) << " port: " << ntohs(client_.sin_port) << endl;
        return clientfd_;
    }

    void Start()
    {
        while (true)
        {
            int clientfd = Accept();
            pool_->Submit(TcpTask(clientfd));
        }
    }

    ~TcpServerMutil()
    {
        if (sockfd_ >= 0)
            close(sockfd_);
    }

private:
    int port_;
    int sockfd_;
    ThreadPool* pool_;
    string address_;
    struct sockaddr_in server_;
};