// 子线程负责处理

#include <iostream>
#include <cstring>
#include <string>
#include <unistd.h>
#include <pthread.h>
#include <arpa/inet.h>
#include <sys/types.h>
#include <sys/socket.h>

#define SIZE 4096

using namespace std;

class TcpThread
{
public:
    TcpThread(int clientfd)
    :clientfd_(clientfd), tid_(0)
    {
    }
    void Init()
    {
        int ret = pthread_create(&tid_, nullptr, ThreadFunction, (void*)this);
        if(ret != 0)
        {
            cout << "Thread create fail, error message: " << strerror(errno) << endl;
            exit(1); 
        }
    }

    bool Receive()
    {
        char rec_buffer_[SIZE];
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
            return true;
        }
        rec_buffer_[ret] = 0;
        if(strcmp(rec_buffer_, "quit") == 0)
        {
            cout << "bye bye" << endl;
            return true;
        }
        else
            cout << "Message from client#: " << rec_buffer_ << endl;
        return false;
    }

    bool Send()
    {
        char send_buffer_[SIZE];
        send_buffer_[0] = 0;
        cout << "Enter the message for client#: ";
        cin >> send_buffer_;
        ssize_t ret = send(clientfd_, send_buffer_, strlen(send_buffer_), 0);
        if (ret < 0)
        {
            cout << "Send Failed" << endl;
            exit(1);
        }
        else if(ret == 0)
        {
            cout << "Client close" << endl;
            return true;
        }
        if(strcmp(send_buffer_, "quit") == 0)
        {
            cout << "bye bye" << endl;
            return true;
        }
        return false;
    }

    static void* ThreadFunction(void* arg)
    {
        TcpThread* tt = (TcpThread*)arg;
        bool flag1 = false;
        bool flag2 = false;
        while(true)
        {
            flag1 = tt->Receive();
            if(flag1 || flag2)
                break;
            flag2 = tt->Send();
        }
        cout << "Communicate End" << endl;
        delete tt;
        return nullptr;
    }

    void Detach()
    {
        pthread_detach(tid_);
    }

    ~TcpThread()
    {
        close(clientfd_);
    }
private:
    int clientfd_;
    pthread_t tid_;
};