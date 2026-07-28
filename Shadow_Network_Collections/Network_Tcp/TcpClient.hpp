// socket connect send recv
#include <iostream>
#include <cstring>
#include <string>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/types.h>
#include <sys/socket.h>

using namespace std;

#define Port 8080
#define Address "127.0.0.1"
#define SIZE 4096

class TcpClient
{
public:
    TcpClient(int port = Port, string address = Address)
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
        send_buffer_[0] = 0;
        cout << "Enter the message for server#: ";
        cin >> send_buffer_;
        ssize_t ret = send(sockfd_, send_buffer_, strlen(send_buffer_), 0);
        if (ret < 0)
        {
            cout << "Send failed, error message: " << strerror(errno) << endl;
            exit(1);
        }
    }

    void Receive()
    {
        rec_buffer_[0] = 0;
        ssize_t ret = recv(sockfd_, rec_buffer_, sizeof(rec_buffer_), 0);
        if (ret < 0)
        {
            cout << "Receive failed, error message: " << strerror(errno) << endl;
            exit(1);
        }
        cout << "Message from Server#: " << rec_buffer_ << endl;
    }

    void Run()
    {
        Connect();
        while (true)
        {
            Send();
            if(strcmp(send_buffer_, "quit") == 0 || strcmp(rec_buffer_, "quit") == 0)
                break;
            Receive();
        }
    }

    ~TcpClient()
    {
        if (sockfd_ >= 0)
            close(sockfd_);
    }

private:
    char rec_buffer_[SIZE];
    char send_buffer_[SIZE];
    int sockfd_;
    int port_;
    string address_;
    struct sockaddr_in server_;
};