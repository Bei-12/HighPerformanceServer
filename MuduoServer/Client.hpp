// socket connect send recv
#pragma once

#include <iostream>
#include <cstring>
#include <string>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/types.h>
#include <sys/socket.h>

using namespace std;

static const int SERVER_PORT = 8080;
#define Address "127.0.0.1"
#define SIZE 4096

class TcpClient
{
public:
    TcpClient(int port = SERVER_PORT, string address = Address)
        : sockfd_(-1), port_(port), address_(address)
    {
        memset((void *)&server_, 0, sizeof(server_));
        rec_buffer_[0] = 0;
        send_buffer_[0] = 0;
    }

    void Init()
    {
        sockfd_ = socket(AF_INET, SOCK_STREAM, 0);
        if (sockfd_ < 0)
        {
            cout << "Socket Created Failed" << errno << "errno Message:" << strerror(errno) << endl;
            exit(1);
        }
        server_.sin_family = AF_INET;
        server_.sin_port = htons(port_);
        server_.sin_addr.s_addr = inet_addr(address_.c_str());
    }

    void Run()
    {
        Connect();
        while (true)
        {
            Send();
            if (!Receive())
                break;
            if (strcmp(send_buffer_.c_str(), "quit") == 0 || strcmp(rec_buffer_, "quit") == 0)
                return;
        }
    }

    ~TcpClient()
    {
        if (sockfd_ >= 0)
            close(sockfd_);
    }

private:
    void Connect()
    {
        int ret = connect(sockfd_, (struct sockaddr *)&server_, (socklen_t)sizeof(server_));
        if (ret < 0)
        {
            cout << "Connect failed, error message: " << strerror(errno) << endl;
            exit(1);
        }
    }

    void Send()
    {
        send_buffer_.clear();
        cout << "Enter the message for server#: ";
        getline(cin, send_buffer_);
        ssize_t ret = send(sockfd_, send_buffer_.c_str(), send_buffer_.size(), 0);
        if (ret < 0)
        {
            cout << "Send failed, error message: " << strerror(errno) << endl;
            exit(1);
        }
    }

    // void Send()
    // {
    //     send_buffer_.assign(5 * 1024 * 1024, 'A');

        

    //     cout << "total send size: "
    //          << send_buffer_.size()
    //          << endl;

    //     size_t total = 0;

    //     while (total < send_buffer_.size())
    //     {
    //         ssize_t ret = send(
    //             sockfd_,
    //             send_buffer_.data() + total,
    //             send_buffer_.size() - total,
    //             0);

    //         if (ret > 0)
    //         {
    //             total += ret;
    //             cout << "client send: "
    //                  << ret
    //                  << " total: "
    //                  << total
    //                  << endl;
    //         }
    //         else if (ret < 0)
    //         {
    //             cout << "send error: "
    //                  << strerror(errno)
    //                  << endl;
    //             break;
    //         }
            
    //     }
        
    //     cout << "client send finished" << endl;
    //     sleep(5);
    // }

    bool Receive()
    {
        rec_buffer_[0] = 0;
        ssize_t ret = recv(sockfd_, rec_buffer_, sizeof(rec_buffer_) - 1, 0);
        if (ret < 0)
        {
            cout << "Receive failed, error message: " << strerror(errno) << endl;
            exit(1);
        }
        else if (ret == 0)
        {
            cout << "Server close" << endl;
            return false;
        }
        rec_buffer_[ret] = 0;
        cout << "Message from Server#: " << rec_buffer_ << endl;
        return true;
    }

private:
    char rec_buffer_[SIZE];
    int sockfd_;
    int port_;
    string address_;
    string send_buffer_;
    struct sockaddr_in server_;
};