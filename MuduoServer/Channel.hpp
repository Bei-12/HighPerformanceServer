// fd事件管理 --- 一个文件描述符在Reactor中的管理对象
// 读 写 都用回调实现
// 成为fd和事件的桥梁
// Connection拥有Channel
// Channel负责修改自己的状态 --- 用回调函数通知Epoller
// 第七阶段：完胜Channel的生命周期状态，让ADD MOD DEL不再靠外部 “猜”，而是根据Channel当前状态决定
// Channel是否已经在epoll中
// 先让内核操作成功，再修改Channel状态

#pragma once

#include <iostream>
#include <cstring>
#include <functional>
#include <unordered_map>
#include <vector>
#include <sys/epoll.h>
#include <sys/socket.h>

using namespace std;

enum ChannelState
{
    NEW = 100,
    ADDED = 200,
    DELETED = 300
};

enum CloseReason
{
    RECV_ZERO,
    WRITE_ZERO,
    RECV_FATAL_ERROR,
    SEND_FATAL_ERROR,
    CHANNEL_HUP,
    CHANNEL_ERROR,
    CHANNEL_RDHUP
};

class Channel
{
public:
    Channel()
        : fd_(-1), events_(0), revents_(0), state_(NEW)
    {
    }

    Channel(int fd)
        : fd_(fd), events_(0), revents_(0), state_(NEW)
    {
        // cout << "Channel fd: " << fd << endl;
    }

    void HandlerEvent()
    {
        // 将事件存进Connection中，根据事件属性调用不同的函数
        // 判断条件
        // accept的判断条件和可读判断条件一样，要进行甄别 --- 将listenfd和clientfd都用readcallback_, 本质都是可读
        if (revents_ & EPOLLIN)
        {
            if (readcallback_)
                readcallback_();
        }
        if (revents_ & EPOLLOUT)
        {
            if (writecallback_)
                writecallback_();
        }
        if (revents_ & EPOLLHUP) // 可能会有数据
        {
            if (closecallback_)
                closecallback_(CHANNEL_HUP);
        }
        if (revents_ & EPOLLERR)
        {
            int error = 0;
            socklen_t len = sizeof(error);
            getsockopt(fd_, SOL_SOCKET, SO_ERROR, &error, &len);
            // cout << "[CHANNEL EPOLLERR]" << " fd=" << fd_ << " SO_ERROR=" << error << " " << strerror(error) << endl;
            if (closecallback_)
                closecallback_(CHANNEL_ERROR);
        }
        if (revents_ & EPOLLRDHUP)
        {
            if (closecallback_)
                closecallback_(CHANNEL_RDHUP);
        }
    }

    void SetFd(int listenfd)
    {
        fd_ = listenfd;
    }

    void SetCloseCallback(function<void(CloseReason)> close)
    {
        closecallback_ = close;
    }

    void SetReadCallback(function<void()> read)
    {
        readcallback_ = read;
    }

    void SetUpdateCallback(function<void(Channel *)> update)
    {
        updatecallback_ = update;
    }

    void SetWriteCallback(function<void()> write)
    {
        writecallback_ = write;
    }

    void SetState(int state)
    {
        state_ = state;
    }

    void EnableRead() // 只修改自己的状态 + 通知Poller更新内核
    {
        events_ |= EPOLLIN;
        Update();
    }

    void EnableWrite()
    {
        events_ |= EPOLLOUT;
        Update();
    }

    void DisableWrite()
    {
        events_ &= ~EPOLLOUT; // 将状态改为可读
        Update();
    }

    void SetRevents(uint32_t event) // 通过Epoller设置实际发生了的事件的状态
    {
        revents_ = event;
    }

    uint32_t GetEvents() // 返回给Epoller读取
    {
        return events_;
    }

    int Fd()
    {
        return fd_;
    }

    int State()
    {
        return state_;
    }

    void Update()
    {
        if (updatecallback_)
            updatecallback_(this);
    }

    // 关闭回调
    ~Channel()
    {
        // 和TcpConnection保存的是同一个fd,TcpConnection关闭fd,Channel中不关闭fd
    }

private:
    int fd_;
    int state_;
    uint32_t events_;  // 希望内核监听什么
    uint32_t revents_; // 由Epoller来设置
    function<void(CloseReason)> closecallback_;
    function<void()> readcallback_;
    function<void(Channel *)> updatecallback_; // 只起一个通知的作用
    function<void()> writecallback_;
};