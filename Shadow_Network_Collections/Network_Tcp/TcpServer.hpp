// socket bind listen accept recv send
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

class TcpServer
{
public:
    TcpServer(int port = Port, string address = Address)
        : clientfd_(-1), sockfd_(-1), port_(port), address_(address)
    {
        memset((void *)&server_, 0, sizeof(server_));
        memset((void *)&client_, 0, sizeof(client_));
        send_buffer_[0] = 0;
        rec_buffer_[0] = 0;
    }

    void Init()
    {
        // socket bind listen
        sockfd_ = socket(AF_INET, SOCK_STREAM, 0);
        if (sockfd_ < 0)
        {
            cout << "Socket Created Failed" << errno << "errno Message:" << strerror(errno) << endl;
            exit(1);
        }

        server_.sin_family = AF_INET;
        server_.sin_port = htons(port_);
        server_.sin_addr.s_addr = inet_addr(address_.c_str());
        int re = bind(sockfd_, (struct sockaddr *)&server_, (socklen_t)sizeof(server_));
        if (re < 0)
        {
            cout << "Bind Failed, errno message: " << strerror(errno) << endl;
            exit(1);
        }
        // listen --- 进入监听状态
        re = listen(sockfd_, 5); // 把socket设置成监听状态，调用一次
        if (re < 0)
        {
            cout << "Bind Failed, errno message: " << strerror(errno) << endl;
            exit(1);
        }
    }

    void Accept()
    {
        // 接受连接请求，并创建一个用于通信的新sockfd
        // 接受客户端地址信息
        client_.sin_family = AF_INET;
        socklen_t len = (socklen_t)sizeof(client_);
        clientfd_ = accept(sockfd_, (struct sockaddr *)&client_, &len);
        if (clientfd_ < 0)
        {
            cout << "Accept Failed" << errno << "errno Message:" << strerror(errno) << endl;
            exit(1);
        }
        cout << "Client ip: " << inet_ntoa(client_.sin_addr) << " port: " << ntohs(client_.sin_port) << endl;
    }

    void Receive()
    {
        rec_buffer_[0] = 0;
        ssize_t ret = recv(clientfd_, rec_buffer_, sizeof(rec_buffer_), 0);
        if (ret < 0)
        {
            cout << "Receive Failed" << endl;
            exit(1);
        }
        else if(ret == 0)
        {
            cout << "Client close" << endl;
        }
        cout << "Message from client#: " << rec_buffer_ << endl;
    }

    void Send()
    {
        send_buffer_[0] = 0;
        cout << "Enter the message for client#: ";
        cin >> send_buffer_;
        ssize_t ret = send(clientfd_, send_buffer_, strlen(send_buffer_), 0);
        if (ret < 0)
        {
            cout << "Send Failed" << endl;
            exit(1);
        }
    }

    void Run()
    {
        Accept();
        while (true)
        {
            Receive();
            if(strcmp(send_buffer_, "quit") == 0 || strcmp(rec_buffer_, "quit") == 0)
                break;
            Send();
        }
    }

    ~TcpServer()
    {
        if (sockfd_ >= 0)
            close(sockfd_);
        if (clientfd_ >= 0)
            close(clientfd_);
    }

private:
    char send_buffer_[SIZE];
    char rec_buffer_[SIZE];
    int port_;
    int clientfd_;
    int sockfd_;
    string address_;
    struct sockaddr_in server_;
    struct sockaddr_in client_;
};