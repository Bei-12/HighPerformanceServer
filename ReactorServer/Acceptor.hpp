// 监听连接
#pragma once

#include <functional>

#include "Channel.hpp"

using namespace std;

class Acceptor
{
public:
    Acceptor()
    { 
    }

    void AcceptHandler(function<void()> accept)
    {
        channel_.EnableRead(); // listenfd可读代表 --- accept队列有连接
        channel_.SetReadCallback(accept);
    }

    void SetFd(int listenfd)
    {
        listenfd_ = listenfd;
        channel_.SetFd(listenfd_);
    }

    Channel& GetChannel()
    {
        return channel_;
    }

private:
    int listenfd_;
    Channel channel_;
};