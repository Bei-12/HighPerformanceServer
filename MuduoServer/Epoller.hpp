// epoll封装 --- 只负责监听事件
// 从fd管理到对象管理
// 返回对象指针 - Channel对象 - Connection对象 - 处理事件
// 只告诉我谁发生事件，具体如何处理交给对应对象
// Epoller不是单纯调用epoll_ctl,而是负责维护用户态Channel状态和内核epoll的一致性
// Poller负责修改内核状态 --- 发现Channel状态更改后，进行触发，将内核的状态也改变
#pragma once

#include <iostream>
#include <cstring>
#include <unordered_map>
#include <vector>
#include <unistd.h>
#include <sys/epoll.h>

#include "Channel.hpp"

#define NUM 64

using namespace std;

class Epoller
{
public:
    Epoller()
    {
        Create();
    }

    void Create()
    {
        epfd_ = epoll_create(NUM);
        if (epfd_ < 0)
        {
            cout << "epfd create failed" << endl;
            exit(1);
        }
    }

    bool Del(int fd) // 删除
    {
        int ret = epoll_ctl(epfd_, EPOLL_CTL_DEL, fd, nullptr);
        if (ret != 0)
        {
            cout << "Del fd failed, error message: " << strerror(errno) << endl;
            return false;
        }
        return true;
    }

    int EpollWait() // 等待事件，并把发生的事件放入events 一组事件
    {
        // -1 --- 没有事件就一直堵塞
        cout << "Epoll wait 前" << endl;
        while (true)
        {
            int ret = epoll_wait(epfd_, events_, NUM, -1);
            // 只做对于错误的判断以及处理
            if (ret == 0)
            {
                cout << "没有事件存在" << endl;
                exit(1);
            }
            else if (ret < 0)
            {
                if (errno == EINTR)
                    continue;
                cout << "Wait failed, error message: " << strerror(errno) << endl;
                exit(1);
            }
            return ret;
        }
    }

    void AddChannel(Channel *channel) // 传递一个Channel对象
    {
        if (channel->State() == NEW || channel->State() == DELETED)
        {
            // cout << "State --- AddChannel: " << channel->State() << endl;
            int fd = channel->Fd();
            // cout << "epfd: " << epfd_ << " fd:" << fd << endl;
            epoll_event ev;
            ev.data.ptr = channel;
            ev.events = channel->GetEvents();
            int ret = epoll_ctl(epfd_, EPOLL_CTL_ADD, fd, &ev);
            if (ret != 0)
            {
                cout << "Add fd failed, error message: " << strerror(errno) << endl;
                return;
            }
            channels_[fd] = channel;
            channel->SetState(ADDED);
            // cout << "State --- AddChannel: " << channel->State() << endl;
        }
        else if (channel->State() == ADDED)
            return;
    }

    void UpdateChannel(Channel *channel) // 修改epoll监听这个fd事件的集合
    {
        // 对channel的状态需要进行判断吗？
        // 要Channel主动通知
        // cout << "State -- UpdateChannel : " << channel->State() << endl;
        if (channel->State() == ADDED)
        {
            epoll_event event;
            event.data.ptr = channel;
            event.events = channel->GetEvents();
            int ret = epoll_ctl(epfd_, EPOLL_CTL_MOD, channel->Fd(), &event);
            if (ret != 0)
            {
                cout << "Change error, error message: " << strerror(errno) << endl;
            }
        }
        else if (channel->State() == NEW)
        {
            AddChannel(channel);
        }
        else if (channel->State() == DELETED)
        {
            AddChannel(channel);
        }
    }

    bool RemoveChannel(Channel *channel)
    {
        // cout << "State --- RemoveChannel: " << channel->State() << endl;
        if (channel->State() == ADDED)
        {
            int fd = channel->Fd();
            if (Del(fd))
            {
                channels_.erase(fd);
                channel->SetState(DELETED);
                // cout << "State --- RemoveChannel: " << channel->State() << endl;
                return true;
            }
        }
        else if (channel->State() == NEW) // 说明根本没进epoll --- 什么都不做
            return false;
        else if (channel->State() == DELETED)
            return false;
    }

    // Q:为什么Poll的处理思路是这样？
    vector<Channel *> Poll() // 收集发生事件的Channel
    {
        int num = EpollWait(); // 说明哪些Channel对应的fd发生事件
        // 获取事件
        vector<Channel *> actives;
        for (int i = 0; i < num; i++)
        {
            // TODO
            auto ch = static_cast<Channel *>(events_[i].data.ptr);
            // 更新对象状态
            ch->SetRevents(events_[i].events);
            // 将发生事件添加到收集发生事件的集合中去
            actives.push_back(ch);
        }
        return actives;
    }

    ~Epoller()
    {
        close(epfd_);
    }

private:
    int epfd_;
    unordered_map<int, Channel *> channels_; // 初始化在哪里进行？ --- 不需要提前初始化
    struct epoll_event events_[NUM];
};
