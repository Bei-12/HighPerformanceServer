// 事件循环
// 不断等待事件
// 不断检测事件，然后分发事件的对象
// 让EventLoop能被主动唤醒，并且安全地从epoll_wait(-1)中退出

#pragma once

#include <atomic>
#include <sys/eventfd.h>

#include "Epoller.hpp"

using namespace std;

// 等待事件
// 分发事件
// 控制循环生命周期

class EventLoop
{
public:
    EventLoop()
        : running_(false)
    {
        wakefd_ = eventfd(0, EFD_NONBLOCK | EFD_CLOEXEC);
        if (wakefd_ >= 0)
        {
            wakechannel_.SetFd(wakefd_);
            wakechannel_.EnableRead();
            wakechannel_.SetReadCallback([this]
                                         { this->WakeUpHandler(); });
            AddChannel(&wakechannel_);
        }
        else
        {
            cout << "Error message#: " << strerror(errno) << endl;
            exit(1);
        }
    }

    void AddChannel(Channel *channel)
    {
        channel->SetUpdateCallback([this](Channel *ch)
                                   { this->UpdateChannel(ch); });
        poller_.AddChannel(channel);
    }

    void Distribute()
    {
        // 活跃Channel集合，返回发生事件的Channel
        vector<Channel *> channels_ = poller_.Poll(); // 每一轮都会变化，是临时变量
        for (auto &channel : channels_)
        {
            channel->HandlerEvent();
        }
    }

    // 如何让EventLoop退出
    // 如果EventLoop此刻阻塞在epoll_wait，怎么把它叫醒
    void Loop()
    {
        cout << "Loop start" << endl;
        running_ = true;
        while (running_)
        {
            Distribute();
            // 停止更改状态的部分
            // TODO
            if (cleanup_)
                cleanup_();
        }
        cout << "Loop End" << endl;
    }

    void Quit()
    {
        running_ = false;
        WakeUp();
    }

    void SetCleanUp(function<void()> clean)
    {
        cleanup_ = clean;
    }

    void UpdateChannel(Channel *channel) // 接受变化的Channel，交给Epoller修改
    {
        poller_.UpdateChannel(channel);
    }

    // 制造一个epoll能感知的事件 --- epoll可以监听到的人造事件 --- eventfd
    // Q：如何制造？
    // ⭐⭐⭐⭐⭐
    void WakeUp()
    {
        // write wakefd_;
        cout << "WakeUp write 成功" << endl;
        while (true)
        {
            uint64_t one = 1;
            ssize_t ret = write(wakefd_, &one, sizeof(one));
            if (ret <= 0)
            {
                if (errno == EINTR)
                    continue;
                else if (errno == EAGAIN)
                    return;
                else
                {
                    cout << "Error message#: " << strerror(errno) << endl;
                    return;
                }
            }
            else
                return;
        }
    }

    // ⭐⭐⭐⭐⭐
    void WakeUpHandler()
    {
        // read wakefd_
        cout << "WakeHandler调用成功" << endl;
        while (true)
        {
            uint64_t value;
            ssize_t ret = read(wakefd_, &value, sizeof(value));
            if (ret <= 0)
            {
                if (errno == EINTR)
                    continue;
                else if (errno == EAGAIN)
                    return;
                else
                {
                    cout << "Error message#: " << strerror(errno) << endl;
                    return;
                }
            }
            else
                return;
        }
    }

    Epoller &GetEpoll()
    {
        return poller_;
    }

    ~EventLoop()
    {
        poller_.RemoveChannel(&wakechannel_);
        close(wakefd_);
    }

private:
    atomic<bool> running_; // 控制while循环
    int wakefd_;           // 一个专门用于线程/事件循环通知的fd
    function<void()> cleanup_;
    Channel wakechannel_;
    Epoller poller_;
};