// epoll告诉我谁有事件，处理一次，不因为某个fd停止整个事件循环
// 阶段5目标：
// 任意一个客户端不能阻塞服务器
// 一个fd处理不完，不能影响其他fd
// 所有IO操作立即返回
// 通过事件驱动继续推进
// 非阻塞模式，有数据/没有数据立即返回
// EAGAIN -- 没有数据，可以处理其他事件
// 业务处理：根据收到的消息，生成需要返回给客户端的数据

#include <iostream>
#include <cstring>
#include <fcntl.h>
#include <string>
#include <unordered_map>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/epoll.h>

using namespace std;

#define Port 8081
#define Address "127.0.0.1"
#define SIZE 4096
#define NUM 64

// 状态和数据 用来解决Receive返回值问题
enum ReadResult
{
    CLOSE = -2,
    ERROR,
    AGAIN,
    DATA
};

class Connection;
class Epoller;
class TcpServerMutil;

// 数据保存位置
// outputbuffer作用：保存发送数据 --- 如何保存
class Connection
{
public:
    Connection(int fd)
        : fd_(fd)
    {
    }

    // Q:如何读干净输入缓冲区 --- 循环读 + 追加
    // TODO --- 返回状态的设计 --- 枚举
    int Receive()
    {
        char rec_buffer_[SIZE];
        rec_buffer_[0] = 0;
        ssize_t ret = recv(fd_, rec_buffer_, sizeof(rec_buffer_) - 1, 0);
        if (ret < 0) // 进行详细判断
        {
            if (errno == EAGAIN) // 停止读取，回到epoll_wait
            {
                cout << "数据读取完毕" << endl;
                // TODO
                // 不应该等待事件，而是等待读取缓冲区数据的填入？？？ --- 等待当前这个fd的读取事件处理完成
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

    void Business()
    {
        output_buffer_ += "Server received --- ";
        output_buffer_ += input_buffer_; // 业务处理
        Clear();
    }

    void Clear()
    {
        input_buffer_.clear();
    }

    // 也需要进行非阻塞
    int Send()
    {
        if (output_buffer_.empty())
            return DATA;
        // cout << "output_buffer_ size: " << output_buffer_.size() << endl;
        ssize_t ret = send(fd_, output_buffer_.c_str(), output_buffer_.size(), 0);
        cout << "ret: " << ret;
        if (ret < 0)
        {
            if (errno == EAGAIN) // EAGAIN = 11 是宏，表示再次尝试 --- 需要保存没有发送的数据 --- 如何保存？
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

    string input_buffer_;
    string output_buffer_; // 保存没有发送完的数据 说明还有数据可以发送

private:
    int fd_;
};

class Epoller
{
public:
    void Create()
    {
        epfd_ = epoll_create(NUM);
        if (epfd_ < 0)
        {
            cout << "epfd create failed" << endl;
            exit(1);
        }
    }

    int Add(int &fd) // 添加fd --- 成功调用返回0
    {
        struct epoll_event event_;
        event_.events = EPOLLIN;
        event_.data.fd = fd;
        int ret = epoll_ctl(epfd_, EPOLL_CTL_ADD, fd, &event_);
        if (ret != 0)
        {
            cout << "Add fd failed, error message: " << strerror(errno) << endl;
            return ret;
        }
        return ret;
    }

    void Mod(int fd) // 修改 EPOLLIN --- 将可读事件修改为可读可写
    {
        // cout << "打开EPOLLOUT fd: " << fd << endl;
        struct epoll_event event_;
        event_.events = EPOLLIN | EPOLLOUT;
        event_.data.fd = fd;
        int ret = epoll_ctl(epfd_, EPOLL_CTL_MOD, fd, &event_);
        if (ret != 0)
        {
            cout << "Mod fd failed, error message: " << strerror(errno) << endl;
            return;
        }
    }

    void DisableWrite(int fd)
    {
        struct epoll_event event_;
        event_.events = EPOLLIN;
        event_.data.fd = fd;
        int ret = epoll_ctl(epfd_, EPOLL_CTL_MOD, fd, &event_);
        if (ret != 0)
        {
            cout << "Close fd failed, error message: " << strerror(errno) << endl;
            return;
        }
    }

    void Del(int &fd) // 删除
    {
        struct epoll_event event_;
        event_.events = EPOLLIN;
        event_.data.fd = fd;
        int ret = epoll_ctl(epfd_, EPOLL_CTL_DEL, fd, &event_);
        if (ret != 0)
        {
            cout << "Del fd failed, error message: " << strerror(errno) << endl;
            return;
        }
    }

    int Wait() // 等待事件，并把发生的事件放入events 一组事件
    {
        int ret = epoll_wait(epfd_, events_, 5, -1);
        if (ret > 0)
        {
            cout << "有" << ret << "个事件 --- ";
            for (int i = 0; i < ret; ++i)
            {
                cout << "第" << i + 1 << "个 -> fd" << events_[i].data.fd << " ";
            }
        }
        else if (ret == 0)
            cout << "没有事件存在" << endl;
        else
        {
            cout << "Wait failed, error message: " << strerror(errno) << endl;
        }
        return ret;
    }

    ~Epoller()
    {
        close(epfd_);
    }

    struct epoll_event events_[NUM];

private:
    int epfd_;
};

class TcpServerMutil
{
public:
    TcpServerMutil()
        : listenfd_(-1), port_(Port), address_(Address)
    {
        memset((void *)&server_, 0, sizeof(server_));
        epoller_.Create();
    }

    void Init()
    {
        // socket bind listen
        // socket创建，设置非阻塞
        listenfd_ = socket(AF_INET, SOCK_STREAM, 0);
        if (listenfd_ < 0)
        {
            cout << "Socket Created Failed" << errno << "errno Message:" << strerror(errno) << endl;
            exit(1);
        }

        // 设置非阻塞
        int flag = fcntl(listenfd_, F_GETFL);
        fcntl(listenfd_, F_SETFL, flag | O_NONBLOCK);

        server_.sin_family = AF_INET;
        server_.sin_port = htons(port_);
        server_.sin_addr.s_addr = inet_addr(address_.c_str());
        int re = bind(listenfd_, (struct sockaddr *)&server_, (socklen_t)sizeof(server_));
        if (re < 0)
        {
            cout << "Bind Failed, errno message: " << strerror(errno) << endl;
            exit(1);
        }

        re = listen(listenfd_, 5);
        if (re < 0)
        {
            cout << "Bind Failed, errno message: " << strerror(errno) << endl;
            exit(1);
        }

        // 加入epoll
        re = epoller_.Add(listenfd_);
        if (re == 0)
            cout << "Add success" << endl;
        else
        {
            cout << "Add failed" << endl;
            return;
        }
    }

    // Accept() -> 生成任务
    // 非阻塞 有连接就生成任务，没有连接，就返回，不等待
    // accept得到clientfd, 设置clientfd非阻塞，加入epoll
    int Accept()
    {
        sockaddr_in client_;
        memset((void *)&client_, 0, sizeof(client_));
        client_.sin_family = AF_INET;
        socklen_t len = (socklen_t)sizeof(client_);
        int clientfd_ = accept(listenfd_, (struct sockaddr *)&client_, &len);
        if (clientfd_ < 0)
        {
            if (errno == EAGAIN)
            {
                return -1;
            }
            else
            {
                cout << "Accept Failed" << errno << "errno Message:" << strerror(errno) << endl;
                exit(1);
            }
        }

        // 设置非阻塞
        int flag = fcntl(clientfd_, F_GETFL);
        fcntl(clientfd_, F_SETFL, flag | O_NONBLOCK);

        // 将clientfd存储在connector_中
        connector_.emplace(clientfd_, Connection(clientfd_));

        cout << "  Client ip: " << inet_ntoa(client_.sin_addr) << " port: " << ntohs(client_.sin_port) << endl;
        return clientfd_;
    }

    // 在start中事件触发后，将读取缓冲区中的数据全部读完
    void ReadHandler(int clientfd)
    {
        auto it = connector_.find(clientfd);
        // 进行数据的读取
        while (true)
        {
            int status = it->second.Receive();
            bool flag = false;
            switch (status)
            {
            case DATA:
                // 循环接受数据
                continue;
            case CLOSE:
                it->second.Clear();
                epoller_.Del(clientfd);
                close(clientfd);
                connector_.erase(clientfd);
                return; // 关闭连接，退出，结束fd处理
            case ERROR:
                cout << "Error message: " << strerror(errno) << endl;
                it->second.Clear();
                epoller_.Del(clientfd);
                close(clientfd);
                connector_.erase(clientfd);
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
        cout << "Message from client#: " << it->second.input_buffer_ << endl;
        if (!it->second.input_buffer_.empty()) // 说明数据没有发完
        {
            it->second.Business();
            if (!it->second.output_buffer_.empty())
                epoller_.Mod(clientfd);
        }
        return;
    }

    void WriteHandler(int clientfd)
    {
        // 循环将没有发送完的数据发完
        // cout << "进入WriteHandler fd: " << clientfd << endl;
        auto it = connector_.find(clientfd);
        while (true)
        {
            int status = it->second.Send();
            int flag = false;
            switch (status)
            {
            case DATA: // 表示数据发送完成
                epoller_.DisableWrite(clientfd);
                return;
            case AGAIN:
                // 等待处理下一次事件
                flag = true;
                break;
            case ERROR:
                cout << "Error message: " << strerror(errno) << endl;
                it->second.Clear();
                epoller_.Del(clientfd);
                close(clientfd);
                connector_.erase(clientfd);
                return;
            case CLOSE:
                it->second.Clear();
                epoller_.Del(clientfd);
                close(clientfd);
                connector_.erase(clientfd);
                return;
            }
            if (flag)
                break;
        }
        cout << "Message send to client#: " << it->second.input_buffer_ << endl;
        return;
    }

    // Epoller等待时间，获得若干就绪fd, 逐个处理
    void Start() // TODO
    {
        int clientfd, num;
        // 得到已经就绪的事件的数量，逐个处理
        while (true)
        {
            num = epoller_.Wait();
            if (num <= 0)
                return;
            for (int i = 0; i < num; ++i)
            {
                if (epoller_.events_[i].data.fd == listenfd_)
                {
                    // 新连接到来 accept clientfd 加入epoller
                    // Accept循环在这里
                    // 有意义，在while中的判断，并不影响clientfd会拿到accept的返回值
                    while (true) // TODO
                    {
                        clientfd = Accept();
                        if (clientfd == -1)
                        {
                            cout << "Accept完成" << endl;
                            break;
                        }
                        if (epoller_.Add(clientfd) != 0)
                            close(clientfd);
                    }
                }
                else // 可读还是可写应该由内核来告诉用户
                {
                    // 不等于 客户端通信 receiv send
                    if (epoller_.events_[i].events & EPOLLIN)
                    {
                        // cout << "读事件" << endl;
                        ReadHandler(epoller_.events_[i].data.fd);
                        auto it = connector_.find(epoller_.events_[i].data.fd);
                        if (it == connector_.end()) // 被删除了
                            continue;
                        if (!it->second.output_buffer_.empty()) // 说明数据没有发完
                            epoller_.Mod(epoller_.events_[i].data.fd);
                    }
                    if (epoller_.events_[i].events & EPOLLOUT)
                    {
                        // cout << "写事件" << endl;
                        WriteHandler(epoller_.events_[i].data.fd);
                    }
                }
            }
        }
    }

    ~TcpServerMutil()
    {
        if (listenfd_ >= 0)
            close(listenfd_);
    }

private:
    int port_;
    int listenfd_;
    string address_;
    struct sockaddr_in server_;
    unordered_map<int, Connection> connector_;
    Epoller epoller_;
};