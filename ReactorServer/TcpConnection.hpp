// Tcp连接对象
// 把Channel的回调绑定到自己的读写处理函数，Channel负责根据实际发生的事件调用对应回调
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

#include "Channel.hpp"

using namespace std;

#define Port 8081
#define Address "127.0.0.1"
#define SIZE 4096
#define NUM 64

enum ReadResult
{
    CLOSE = -2,
    ERROR,
    AGAIN,
    DATA
};

// 数据保存位置
// outputbuffer作用：保存发送数据 --- 如何保存
class TcpConnection
{
public:
    TcpConnection(int fd)
        : fd_(fd), channel_(fd)
    {
        ReadCall();
        WriteCall();
        channel_.EnableRead();
    }

    // Q:如何读干净输入缓冲区 --- 循环读 + 追加
    int Read()
    {
        char rec_buffer_[SIZE];
        rec_buffer_[0] = 0;
        ssize_t ret = recv(fd_, rec_buffer_, sizeof(rec_buffer_) - 1, 0);
        if (ret < 0) // 进行详细判断
        {
            if (errno == EAGAIN) // 停止读取，回到epoll_wait
            {
                cout << "数据读取完毕" << endl;
                return AGAIN;
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
        rec_buffer_[ret] = 0;
        input_buffer_ += (string)rec_buffer_;
        return DATA;
    }

    int Write()
    {
        if (output_buffer_.empty())
            return DATA;
        ssize_t ret = send(fd_, output_buffer_.c_str(), output_buffer_.size(), 0);
        cout << "ret: " << ret;
        if (ret < 0)
        {
            if (errno == EAGAIN)
            {
                cout << "发送缓冲区满了，暂时不能继续发送" << endl;
                return AGAIN;
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
            if ((size_t)ret != output_buffer_.size())
            {
                output_buffer_.erase(0, ret);
                return AGAIN;
            }
            else
            {
                output_buffer_.clear();
                return DATA;
            }
        }
    }

    void Business()
    {
        output_buffer_ += "Server received --- ";
        output_buffer_ += input_buffer_;
        Clear();
    }

    void Clear()
    {
        input_buffer_.clear();
    }

    // Q:如何将function包装的函数传递给Channel
    // lambda将读函数存储在其中，通过function传递给Channel，完成回调
    void ReadCall()
    {
        auto read = [this]
        {
            this->ReadHandler();
        };
        channel_.SetReadCallback(read);
    }

    void WriteCall()
    {
        auto write = [this]
        {
            this->WriteHandler();
        };
        channel_.SetWriteCallback(write);
    }

    void TriggerClose(int fd_) // 执行回调
    {
        if (closecallback_) // 进行判空处理
            closecallback_(fd_);
    }

    void ErrorCall()
    {
        auto error = [this]
        {
            this->ErrorHandler();
        };
        channel_.SetErrorCallback(error);
    }

    void ErrorHandler()
    {
        close(fd_);
        input_buffer_.clear();
        output_buffer_.clear();
    }

    void ReadHandler()
    {
        // 进行数据的读取
        while (true)
        {
            int status = Read();
            bool flag = false;
            switch (status)
            {
            case DATA:
                // 循环接受数据
                continue;
            case CLOSE:
                Clear();
                TriggerClose(fd_);
                return; // 关闭连接，退出，结束fd处理
            case ERROR:
                cout << "Error message: " << strerror(errno) << endl;
                Clear();
                ErrorCall();
                return;
            case AGAIN:
                flag = true;
                break;
            default:
                break;
            }
            if (flag)
                break;
        }
        cout << "Message from client#: " << input_buffer_ << endl;
        if (!input_buffer_.empty()) // 说明数据没有发完
        {
            Business();
            if (!output_buffer_.empty()) // 说明还有数据没有读完
            {
                // 监听事件进行修改
                channel_.EnableWrite();
            }
        }
        return;
    }

    void WriteHandler()
    {
        // 循环将没有发送完的数据发完
        while (true)
        {
            int status = Write();
            int flag = false;
            switch (status)
            {
            case DATA: // 表示数据发送完成
                // 告诉Channel，然后Channel来调用epoller来更改事件状态
                channel_.DisableWrite();
                return;
            case AGAIN:
                flag = true;
                break;
            case ERROR:
                cout << "Error message: " << strerror(errno) << endl;
                Clear();
                ErrorCall();
                return;
            case CLOSE:
                Clear();
                TriggerClose(fd_);
                return;
            }
            if (flag)
                break;
        }
        cout << "Message send to client#: " << input_buffer_ << endl;
        return;
    }

    void SetCloseCallback(function<void(int)> close)
    {
        closecallback_ = close;
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
    string input_buffer_;
    string output_buffer_;
    Channel channel_;
};
