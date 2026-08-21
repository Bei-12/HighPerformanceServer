// 事件循环
// 不断等待事件
// 不断检测事件，然后分发事件的对象
#pragma once
#include "Epoller.hpp"

using namespace std;

// 等待事件
// 分发事件
// 控制循环生命周期

class EventLoop
{
public:
    EventLoop()
    :running_(false)
    {
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

    void Loop()
    {
        running_ = true;
        while (running_)
        {
            Distribute();
            // 停止更改状态的部分
            // TODO
        }
    }

    void Quit()
    {
        running_ = false;
    }

    void UpdateChannel(Channel* channel) // 接受变化的Channel，交给Epoller修改
    {
        poller_.UpdateChannel(channel);
    }

    void AddChannel(Channel* channel)
    {
        channel->SetUpdateCallback([this](Channel* ch){this->UpdateChannel(ch);});
        poller_.AddChannel(channel);
    }

    Epoller& GetEpoll()
    {
        return poller_;
    }

private:
    bool running_; // 控制while循环
    Epoller poller_;
};