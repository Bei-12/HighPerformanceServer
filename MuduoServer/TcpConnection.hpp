// Tcp连接对象
// 把Channel的回调绑定到自己的读写处理函数，Channel负责根据实际发生的事件调用对应回调
// 只发出 “关闭请求” --- 不立刻删除自己
// 在非阻塞socket下，读要读到不能再读，写要写到不能再写，剩余数据交给buffer和EPOLLIN/EPOLLPUT管理

#pragma once

#include <iostream>
#include <cstring>
#include <string>
#include <unordered_map>
#include <fcntl.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/epoll.h>

#include "Buffer.hpp"
#include "Channel.hpp"

using namespace std;

#define Port 8080
#define Address "127.0.0.1"
#define SIZE 4096
#define NUM 64

enum ConnectionState // 这个TCP连接现在是否还允许继续处理业务
{
    CONNECTED = -10,
    CLOSING = -9,
    CLOSED = -8
};

enum ReadResult
{
    CLOSE = -3,
    ERROR = -2,
    RETRY = -1,
    AGAIN = 0,
    DATA = 1
};

// 数据保存位置
// outputbuffer作用：保存发送数据 --- 如何保存
class TcpConnection
{
public:
    TcpConnection(int fd)
        : fd_(fd), channel_(fd), state_(CONNECTED)
    {
        CloseCall();
        ReadCall();
        WriteCall();
        channel_.EnableRead();
    }

    // Q:如何读干净输入缓冲区 --- 循环读 + 追加
    // Q:read/write自己发现CLOSE/ERROR时，没有真正进入关闭流程
    int Read()
    {
        char rec_buffer_[SIZE];
        rec_buffer_[0] = 0;
        ssize_t ret = recv(fd_, rec_buffer_, sizeof(rec_buffer_), 0);
        if (ret < 0) // 进行详细判断
        {
            if (errno == EAGAIN || errno == EWOULDBLOCK) // 停止读取，回到epoll_wait
            {
                cout << "数据读取完毕" << endl;
                return AGAIN;
            }
            else if (errno == EINTR)
            {
                cout << "被信号打断" << endl;
                return RETRY; // 可以重试
            }
            else
            {
                cout << "Mistake message: " << strerror(errno) << endl;
                return ERROR;
            }
        }
        else if (ret == 0) // 对端关闭
        {
            cout << "Client close" << endl;
            return CLOSE;
        }
        input_buffer_.Append(rec_buffer_, ret);
        return DATA;
    }

    int Write()
    {
        if (output_buffer_.Empty())
            return DATA;
        ssize_t ret = send(fd_, output_buffer_.GetString().c_str(), output_buffer_.Size(), 0);
        cout << "send ret: " << ret;
        if (ret < 0)
        {
            if (errno == EAGAIN || errno == EWOULDBLOCK)
            {
                cout << "发送缓冲区满了，暂时不能继续发送" << endl;
                return AGAIN;
            }
            else if (errno == EINTR)
            {
                cout << "被信号打断" << endl;
                return RETRY;
            }
            else if (errno == EPIPE)
            {
                cout << "对端已无法接收" << endl;
                return ERROR;
            }
            else
            {
                cout << "Send Failed" << endl;
                return ERROR;
            }
        }
        else if (ret == 0)
        {
            cout << "Server close" << endl;
            return CLOSE;
        }
        else
        {
            if ((size_t)ret != output_buffer_.Size())
            {
                output_buffer_.Retrieve(ret);
                return AGAIN;
            }
            else
            {
                output_buffer_.RetrieveAll();
                return DATA;
            }
        }
    }

    void Business()
    {
        output_buffer_.Append("Server received --- ");
        output_buffer_.Append(input_buffer_.GetString());
        Clear();
    }

    void Clear()
    {
        input_buffer_.RetrieveAll();
    }

    // Q:如何将function包装的函数传递给Channel
    // lambda将读函数存储在其中，通过function传递给Channel，完成回调

    void CloseCall()
    {
        auto close = [this]
        {
            this->CloseHandler();
        };
        channel_.SetCloseCallback(close);
    }

    void CloseHandler() // 通知TcpServer fd应该删除
    {
        if (state_ == CONNECTED)
        {
            SetState(CLOSING);
            TriggerClose(fd_);
        }
    }

    void ReadCall()
    {
        auto read = [this]
        {
            this->ReadHandler();
        };
        channel_.SetReadCallback(read);
    }

    void ReadHandler()
    {
        // 进行数据的读取
        while (true)
        {
            if (state_ == CONNECTED)
            {
                int status = Read();
                bool flag = false;
                switch (status)
                {
                case DATA:
                    // 循环接受数据
                    continue;
                case CLOSE:
                    CloseHandler();
                    return; // 关闭连接，退出，结束fd处理
                case ERROR:
                    cout << "Error message: " << strerror(errno) << endl;
                    CloseHandler();
                    return;
                case AGAIN:
                    flag = true;
                    break;
                case RETRY:
                    continue;
                default:
                    break;
                }
                if (flag)
                    break;
            }
            else
                break;
        }

        // cout << "Message from client#: " << input_buffer_ << endl;
        if (!input_buffer_.Empty()) // 说明数据没有发完
        {
            Business();
            if (!output_buffer_.Empty()) // 说明还有数据没有读完
            {
                // 监听事件进行修改
                channel_.EnableWrite();
            }
        }
        return;
    }

    void SetCloseCallback(function<void(int)> close)
    {
        closecallback_ = close;
    }

    void SetState(ConnectionState state)
    {
        state_ = state;
    }

    void TriggerClose(int fd_) // 执行回调
    {
        if (closecallback_) // 进行判空处理
            closecallback_(fd_);
    }

    void WriteCall()
    {
        auto write = [this]
        {
            this->WriteHandler();
        };
        channel_.SetWriteCallback(write);
    }

    void WriteHandler()
    {
        // 循环将没有发送完的数据发完
        while (true)
        {
            if (state_ == CONNECTED)
            {
                int status = Write();
                int flag = false;
                switch (status)
                {
                case AGAIN:
                    flag = true;
                    break;
                case CLOSE:
                    CloseHandler();
                    return;
                case DATA: // 表示数据发送完成
                    // 告诉Channel，然后Channel来调用epoller来更改事件状态
                    channel_.DisableWrite();
                    return;
                case ERROR:
                    cout << "Error message: " << strerror(errno) << endl;
                    CloseHandler();
                    return;
                case RETRY:
                    continue;
                }
                if (flag)
                    break;
            }
            else
                break;
        }
        cout << "Message send to client#: " << input_buffer_.GetString() << endl;
        return;
    }

    Channel &GetChannel()
    {
        return channel_;
    }

    ~TcpConnection()
    {
        // 在TcpServer中会进行关闭
        close(fd_);
    }

private:
    int fd_;
    function<void(int)> closecallback_;
    Buffer input_buffer_;
    Buffer output_buffer_;
    Channel channel_;
    ConnectionState state_;
};
