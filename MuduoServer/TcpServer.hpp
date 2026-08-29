// 业务线程
#pragma once

#include <algorithm>

#include "Acceptor.hpp"
#include "EventLoop.hpp"
#include "TcpConnection.hpp"

using namespace std;

static const int SERVER_PORT = 8080;
#define Address "127.0.0.1"

class TcpServer
{
public:
    TcpServer()
        : listenfd_(-1), port_(SERVER_PORT), address_(Address)
    {
        CleanCall();
    }

    void CleanCall()
    {
        auto clean = [this]
        {
            this->CleanUp();
        };
        loop_.SetCleanUp(clean);
    }

    void CleanUp()
    {
        for (auto &fd : Fds_)
        {
            auto it = connections_.find(fd);
            if (it != connections_.end())
            {
                it->second->SetState(CLOSED);
                delete it->second;
                connections_.erase(it);
            }
        }
        Fds_.clear();
    }

    void Init()
    {
        // socket bind listen
        // socket创建，设置非阻塞
        listenfd_ = socket(AF_INET, SOCK_STREAM, 0);
        if (listenfd_ < 0)
        {
            cout << "Socket Created Failed" << errno << "errno Message:" << strerror(errno) << endl;
            exit(1);
        }

        // 设置非阻塞
        int flag = fcntl(listenfd_, F_GETFL);
        fcntl(listenfd_, F_SETFL, flag | O_NONBLOCK);

        server_.sin_family = AF_INET;
        server_.sin_port = htons(port_);
        server_.sin_addr.s_addr = inet_addr(address_.c_str());
        int re = bind(listenfd_, (struct sockaddr *)&server_, (socklen_t)sizeof(server_));
        if (re < 0)
        {
            cout << "Bind Failed, errno message: " << strerror(errno) << endl;
            exit(1);
        }

        re = listen(listenfd_, NUM);
        if (re < 0)
        {
            cout << "Bind Failed, errno message: " << strerror(errno) << endl;
            exit(1);
        }

        AcceptCall();
        // 将listenfd事件加入Epoller中
        acceptor_.SetFd(listenfd_);
        loop_.AddChannel(&(acceptor_.GetChannel()));
        acceptor_.AcceptHandler(AcceptCall());
    }

    void Start()
    {
        Init();
        loop_.Loop();
    }

    void AddTcpConnection(int clientfd)
    {
        TcpConnection *conn = new TcpConnection(clientfd);
        conn->SetCloseCallback([this](int fd)
                               { this->RemoveConnection(fd); });
        conn->SetState(CONNECTED);
        connections_.emplace(clientfd, conn);
        // 获取Channel 加入Poller
        loop_.AddChannel(&(conn->GetChannel()));
    }

    void RemoveConnection(int fd) // 存在fd不存在的情况需要判断
    {
        auto it = connections_.find(fd);
        if (it != connections_.end())
        {
            if (loop_.GetEpoll().RemoveChannel(&(it->second->GetChannel())))
            {
                Fds_.push_back(it->second->GetChannel().Fd()); // 进行去重
                sort(Fds_.begin(), Fds_.end());
                Fds_.erase(unique(Fds_.begin(), Fds_.end()), Fds_.end());
            }
        }
    }

    // Accept() -> 生成任务
    // 非阻塞 有连接就生成任务，没有连接，就返回，不等待
    // accept得到clientfd, 设置clientfd非阻塞，加入epoll
    int Accept()
    {
        while (true)
        {
            sockaddr_in client_;
            memset((void *)&client_, 0, sizeof(client_));
            client_.sin_family = AF_INET;
            socklen_t len = (socklen_t)sizeof(client_);
            int clientfd_ = accept(listenfd_, (struct sockaddr *)&client_, &len);
            if (clientfd_ < 0)
            {
                if (errno == EAGAIN)
                {
                    return -1;
                }
                else
                {
                    cout << "Accept Failed" << errno << "errno Message:" << strerror(errno) << endl;
                    exit(1);
                }
            }

            // 设置非阻塞
            int flag = fcntl(clientfd_, F_GETFL);
            fcntl(clientfd_, F_SETFL, flag | O_NONBLOCK);

            // 创建TCPConnection
            AddTcpConnection(clientfd_);
        }
    }

    function<void()> AcceptCall()
    {
        auto accept = [this]
        {
            this->Accept();
        };
        return accept;
    }

private:
    int listenfd_;
    int port_;
    vector<int> Fds_;
    unordered_map<int, TcpConnection *> connections_; // 管理生命周期
    string address_;
    struct sockaddr_in server_;
    Acceptor acceptor_;
    EventLoop loop_; // 服务器需要运行事件循环
};