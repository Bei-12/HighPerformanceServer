// 通过epoll管理事件
// 监控谁 添加fd --- 关心谁添加谁
// 服务器通过epoll知道处理已经就绪的fd
// fd不用就删除

#include <iostream>
#include <cstring>
#include <string>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/epoll.h>

// 服务器包括线程

using namespace std;

#define Port 8081
#define Address "127.0.0.1"
#define SIZE 4096
#define NUM 64

// 声明类
class Epoller;
class TcpServerMutil;

class Epoller
{
public:
    void Create()
    {
        epfd_ = epoll_create(NUM);
        if (epfd_ < 0)
        {
            cout << "epfd create failed" << endl;
            exit(1);
        }
    }

    int Add(int &fd) // 添加fd --- 成功调用返回0
    {
        struct epoll_event event_;
        event_.events = EPOLLIN;
        event_.data.fd = fd;
        int ret = epoll_ctl(epfd_, EPOLL_CTL_ADD, fd, &event_);
        if (ret != 0)
        {
            cout << "Add fd failed, error message: " << strerror(errno) << endl;
            return ret;
        }
        return ret;
    }

    void Mod(int &fd) // 修改 EPOLLIN --- 可读事件
    {
        struct epoll_event event_;
        event_.events = EPOLLIN;
        event_.data.fd = fd;
        int ret = epoll_ctl(epfd_, EPOLL_CTL_MOD, fd, &event_);
        if (ret != 0)
        {
            cout << "Mod fd failed, error message: " << strerror(errno) << endl;
            return;
        }
    }

    void Del(int &fd) // 删除
    {
        struct epoll_event event_;
        event_.events = EPOLLIN;
        event_.data.fd = fd;
        int ret = epoll_ctl(epfd_, EPOLL_CTL_DEL, fd, &event_);
        if (ret != 0)
        {
            cout << "Del fd failed, error message: " << strerror(errno) << endl;
            return;
        }
    }

    int Wait() // 等待事件，并把发生的事件放入events 一组事件
    {
        int ret = epoll_wait(epfd_, events_, 5, -1);
        if (ret > 0)
        {
            cout << "有" << ret << "个事件" << endl;
            for (int i = 0; i < ret; ++i)
            {
                cout << "第" << i + 1 << "个 -> fd " << events_[i].data.fd;
            }
        }
        else if (ret == 0)
            cout << "没有事件存在" << endl;
        else
        {
            cout << "Wait failed, error message: " << strerror(errno) << endl;
        }
        return ret;
    }

    ~Epoller()
    {
        close(epfd_);
    }

    struct epoll_event events_[NUM];

private:
    int epfd_;
};

class TcpServerMutil
{
public:
    TcpServerMutil()
        : sockfd_(-1), port_(Port), address_(Address)
    {
        memset((void *)&server_, 0, sizeof(server_));
        epoller_.Create();
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

        re = epoller_.Add(sockfd_);
        if (re == 0)
            cout << "Add success" << endl;
        else
        {
            cout << "Add failed" << endl;
            return;
        }
    }

    // Accept() -> 生成任务
    int Accept()
    {
        sockaddr_in client_;
        memset((void *)&client_, 0, sizeof(client_));
        client_.sin_family = AF_INET;
        socklen_t len = (socklen_t)sizeof(client_);
        int clientfd_ = accept(sockfd_, (struct sockaddr *)&client_, &len);
        if (clientfd_ < 0)
        {
            cout << "Accept Failed" << errno << "errno Message:" << strerror(errno) << endl;
            exit(1);
        }
        cout << "Client ip: " << inet_ntoa(client_.sin_addr) << " port: " << ntohs(client_.sin_port) << endl;
        return clientfd_;
    }

    // Epoller等待时间，获得若干就绪fd, 逐个处理
    void Start() // TODO
    {
        // 得到已经就绪的事件的数量，逐个处理
        while (true)
        {
            int num = epoller_.Wait();
            // 处理这一步如何完成实现
            if (num <= 0)
                return;
            for (int i = 0; i < num; ++i)
            {
                if (epoller_.events_[i].data.fd == sockfd_)
                {
                    // 新连接到来 accept clientfd 加入epoller
                    int clientfd = Accept();
                    epoller_.Add(clientfd);
                }
                else
                {
                    // 不等于 客户端通信 receiv send
                    int clientfd = epoller_.events_[i].data.fd;
                    string buffer = Receive(clientfd);
                    if (buffer.empty())
                    {
                        epoller_.Del(clientfd);
                        close(clientfd);
                    }
                    else
                    {
                        Send(clientfd, buffer);
                    }
                }
            }
        }
    }

    string Receive(int clientfd_)
    {
        char rec_buffer_[SIZE];
        rec_buffer_[0] = 0;
        ssize_t ret = recv(clientfd_, rec_buffer_, sizeof(rec_buffer_) - 1, 0);
        if (ret < 0)
        {
            cout << "Receive Failed" << endl;
            return "";
        }
        else if (ret == 0)
        {
            cout << "Client close" << endl;
            return "";
        }
        rec_buffer_[ret] = 0;
        cout << "Message from client#: " << rec_buffer_ << endl;
        return (string)rec_buffer_;
    }

    void Send(int clientfd_, string buffer)
    {
        string send_buffer_ = "Server received --- ";
        send_buffer_ += buffer;
        ssize_t ret = send(clientfd_, send_buffer_.c_str(), send_buffer_.size(), 0);
        if (ret < 0)
        {
            cout << "Send Failed" << endl;
            exit(1);
        }
        else if (ret == 0)
        {
            cout << "Server close" << endl;
            return;
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
    string address_;
    struct sockaddr_in server_;
    Epoller epoller_;
};