// #include <iostream>
// #include <vector>
// #include <unordered_map>
// #include <cstring>
// #include <cerrno>

// #include <unistd.h>
// #include <fcntl.h>
// #include <arpa/inet.h>
// #include <sys/socket.h>
// #include <sys/epoll.h>

// using namespace std;

// const int SERVER_PORT = 8080;
// const char *SERVER_IP = "127.0.0.1";

// const int MAX_EVENTS = 1024;
// const int CONNECTIONS = 20000;

// // 设置非阻塞
// bool SetNonBlock(int fd)
// {
//     int flags = fcntl(fd, F_GETFL, 0);
//     if (flags < 0)
//     {
//         cout << "fcntl F_GETFL failed: "
//              << strerror(errno) << endl;
//         return false;
//     }

//     if (fcntl(fd, F_SETFL, flags | O_NONBLOCK) < 0)
//     {
//         cout << "fcntl F_SETFL failed: "
//              << strerror(errno) << endl;
//         return false;
//     }

//     return true;
// }

// // 判断非阻塞 connect 最终是否成功
// bool CheckConnect(int fd)
// {
//     int error = 0;
//     socklen_t len = sizeof(error);

//     int ret = getsockopt(
//         fd,
//         SOL_SOCKET,
//         SO_ERROR,
//         &error,
//         &len);

//     if (ret < 0)
//     {
//         cout << "getsockopt failed: "
//              << strerror(errno) << endl;
//         return false;
//     }

//     if (error != 0)
//     {
//         return false;
//     }

//     return true;
// }

// int main()
// {
//     int epfd = epoll_create1(0);

//     if (epfd < 0)
//     {
//         cout << "epoll_create1 failed: "
//              << strerror(errno) << endl;
//         return 1;
//     }

//     sockaddr_in server_addr;
//     memset(&server_addr, 0, sizeof(server_addr));

//     server_addr.sin_family = AF_INET;
//     server_addr.sin_port = htons(SERVER_PORT);

//     inet_pton(
//         AF_INET,
//         SERVER_IP,
//         &server_addr.sin_addr);

//     vector<int> all_fds;

//     // 保存正在等待连接完成的socket
//     unordered_map<int, bool> connecting;

//     int success_count = 0;
//     int fail_count = 0;

//     // =========================
//     // 1. 批量创建连接
//     // =========================

//     for (int i = 0; i < CONNECTIONS; ++i)
//     {
//         int fd = socket(
//             AF_INET,
//             SOCK_STREAM,
//             0);

//         if (fd < 0)
//         {
//             cout << "socket failed: "
//                  << strerror(errno) << endl;

//             ++fail_count;
//             continue;
//         }

//         if (!SetNonBlock(fd))
//         {
//             close(fd);
//             ++fail_count;
//             continue;
//         }

//         int ret = connect(
//             fd,
//             reinterpret_cast<sockaddr *>(&server_addr),
//             sizeof(server_addr));

//         if (ret == 0)
//         {
//             // 极少数情况下非阻塞connect可能直接完成
//             ++success_count;
//             all_fds.push_back(fd);

//             cout << "connect immediately success, fd: "
//                  << fd << endl;
//         }
//         else
//         {
//             if (errno == EINPROGRESS)
//             {
//                 // TCP连接正在进行
//                 epoll_event event;
//                 memset(&event, 0, sizeof(event));

//                 event.events =
//                     EPOLLOUT |
//                     EPOLLERR |
//                     EPOLLHUP;

//                 event.data.fd = fd;

//                 if (epoll_ctl(
//                         epfd,
//                         EPOLL_CTL_ADD,
//                         fd,
//                         &event) < 0)
//                 {
//                     cout << "epoll_ctl add failed: "
//                          << strerror(errno) << endl;

//                     close(fd);
//                     ++fail_count;
//                     continue;
//                 }

//                 connecting[fd] = true;
//                 all_fds.push_back(fd);
//             }
//             else
//             {
//                 cout << "connect failed immediately: "
//                      << strerror(errno) << endl;

//                 close(fd);
//                 ++fail_count;
//             }
//         }
//     }

//     cout << endl;
//     cout << "Start waiting connect..." << endl;

//     // =========================
//     // 2. epoll等待连接完成
//     // =========================

//     epoll_event events[MAX_EVENTS];

//     while (!connecting.empty())
//     {
//         int num = epoll_wait(
//             epfd,
//             events,
//             MAX_EVENTS,
//             5000);

//         if (num < 0)
//         {
//             if (errno == EINTR)
//                 continue;

//             cout << "epoll_wait failed: "
//                  << strerror(errno) << endl;

//             break;
//         }

//         if (num == 0)
//         {
//             cout << "connect timeout" << endl;
//             break;
//         }

//         for (int i = 0; i < num; ++i)
//         {
//             int fd = events[i].data.fd;

//             auto it = connecting.find(fd);

//             if (it == connecting.end())
//                 continue;

//             if (CheckConnect(fd))
//             {
//                 ++success_count;

//                 cout << "connect success fd: "
//                      << fd << endl;
//             }
//             else
//             {
//                 ++fail_count;

//                 cout << "connect failed fd: "
//                      << fd << endl;

//                 close(fd);
//             }

//             epoll_ctl(
//                 epfd,
//                 EPOLL_CTL_DEL,
//                 fd,
//                 nullptr);

//             connecting.erase(it);
//         }
//     }

//     // =========================
//     // 3. 输出结果
//     // =========================

//     cout << endl;
//     cout << "========== Stress Result ==========" << endl;

//     cout << "Target connections: "
//          << CONNECTIONS << endl;

//     cout << "Success connections: "
//          << success_count << endl;

//     cout << "Failed connections: "
//          << fail_count << endl;

//     cout << "===================================" << endl;

//     // 保持连接一段时间，方便观察服务器
//     cout << "Keep connections for 30 seconds..."
//          << endl;

//     sleep(30);

//     // =========================
//     // 4. 清理
//     // =========================

//     for (int fd : all_fds)
//     {
//         close(fd);
//     }

//     close(epfd);

//     return 0;
// }

// Part2 -- V1
// #include <iostream>
// #include <vector>
// #include <unordered_map>
// #include <cstring>
// #include <cerrno>

// #include <unistd.h>
// #include <fcntl.h>
// #include <arpa/inet.h>
// #include <sys/socket.h>
// #include <sys/epoll.h>

// using namespace std;

// const char *SERVER_IP = "127.0.0.1";
// const int SERVER_PORT = 8080;

// const int CONNECTIONS = 15000;   // 先1000测试，别直接20000
// const int MAX_EVENTS = 4096;
// const int PAYLOAD_SIZE = 64;

// enum ClientState
// {
//     CONNECTING,
//     SENDING,
//     RECEIVING,
//     FINISHED,
//     FAILED
// };

// struct ClientInfo
// {
//     int fd = -1;

//     ClientState state = CONNECTING;

//     string send_buffer;
//     size_t send_offset = 0;

//     string recv_buffer;
// };

// bool SetNonBlock(int fd)
// {
//     int flags = fcntl(fd, F_GETFL, 0);

//     if (flags < 0)
//         return false;

//     if (fcntl(fd, F_SETFL, flags | O_NONBLOCK) < 0)
//         return false;

//     return true;
// }

// bool CheckConnect(int fd)
// {
//     int error = 0;
//     socklen_t len = sizeof(error);

//     if (getsockopt(
//             fd,
//             SOL_SOCKET,
//             SO_ERROR,
//             &error,
//             &len) < 0)
//     {
//         return false;
//     }

//     if (error != 0)
//     {
//         cout << "connect error: "
//              << strerror(error)
//              << endl;

//         return false;
//     }

//     return true;
// }

// bool ModifyEvent(
//     int epfd,
//     int fd,
//     uint32_t events)
// {
//     epoll_event ev;
//     memset(&ev, 0, sizeof(ev));

//     ev.events = events;
//     ev.data.fd = fd;

//     if (epoll_ctl(
//             epfd,
//             EPOLL_CTL_MOD,
//             fd,
//             &ev) < 0)
//     {
//         cout << "epoll_ctl MOD failed: "
//              << strerror(errno)
//              << endl;

//         return false;
//     }

//     return true;
// }

// int main()
// {
//     int epfd = epoll_create1(0);

//     if (epfd < 0)
//     {
//         cout << "epoll_create1 failed: "
//              << strerror(errno)
//              << endl;

//         return 1;
//     }

//     sockaddr_in server_addr;
//     memset(&server_addr, 0, sizeof(server_addr));

//     server_addr.sin_family = AF_INET;
//     server_addr.sin_port = htons(SERVER_PORT);

//     if (inet_pton(
//             AF_INET,
//             SERVER_IP,
//             &server_addr.sin_addr) <= 0)
//     {
//         cout << "inet_pton failed" << endl;
//         return 1;
//     }

//     unordered_map<int, ClientInfo> clients;

//     int connect_success = 0;
//     int connect_failed = 0;

//     int request_success = 0;
//     int request_failed = 0;

//     // ============================================
//     // 第一阶段：批量创建非阻塞连接
//     // ============================================

//     for (int i = 0; i < CONNECTIONS; ++i)
//     {
//         int fd = socket(
//             AF_INET,
//             SOCK_STREAM,
//             0);

//         if (fd < 0)
//         {
//             ++connect_failed;
//             continue;
//         }

//         if (!SetNonBlock(fd))
//         {
//             close(fd);
//             ++connect_failed;
//             continue;
//         }

//         ClientInfo info;

//         info.fd = fd;
//         info.state = CONNECTING;

//         // 每个客户端发送固定64字节
//         info.send_buffer.assign(
//             PAYLOAD_SIZE,
//             'A');

//         clients.emplace(fd, std::move(info));

//         int ret = connect(
//             fd,
//             reinterpret_cast<sockaddr *>(&server_addr),
//             sizeof(server_addr));

//         if (ret == 0)
//         {
//             ++connect_success;

//             clients[fd].state = SENDING;

//             epoll_event ev;
//             memset(&ev, 0, sizeof(ev));

//             ev.events =
//                 EPOLLOUT |
//                 EPOLLERR |
//                 EPOLLHUP;

//             ev.data.fd = fd;

//             if (epoll_ctl(
//                     epfd,
//                     EPOLL_CTL_ADD,
//                     fd,
//                     &ev) < 0)
//             {
//                 clients[fd].state = FAILED;
//                 ++request_failed;
//             }
//         }
//         else if (errno == EINPROGRESS)
//         {
//             epoll_event ev;
//             memset(&ev, 0, sizeof(ev));

//             ev.events =
//                 EPOLLOUT |
//                 EPOLLERR |
//                 EPOLLHUP;

//             ev.data.fd = fd;

//             if (epoll_ctl(
//                     epfd,
//                     EPOLL_CTL_ADD,
//                     fd,
//                     &ev) < 0)
//             {
//                 clients[fd].state = FAILED;
//                 ++connect_failed;
//             }
//         }
//         else
//         {
//             clients[fd].state = FAILED;
//             ++connect_failed;
//         }
//     }

//     // ============================================
//     // 第二阶段：连接完成 → send → recv
//     // ============================================

//     epoll_event events[MAX_EVENTS];

//     int finished_count = 0;

//     while (finished_count <
//            connect_success + (CONNECTIONS - connect_failed - connect_success))
//     {
//         int num = epoll_wait(
//             epfd,
//             events,
//             MAX_EVENTS,
//             20000);

//         if (num < 0)
//         {
//             if (errno == EINTR)
//                 continue;

//             cout << "epoll_wait failed: "
//                  << strerror(errno)
//                  << endl;

//             break;
//         }

//         if (num == 0)
//         {
//             cout << "stress timeout" << endl;
//             break;
//         }

//         for (int i = 0; i < num; ++i)
//         {
//             int fd = events[i].data.fd;

//             auto it = clients.find(fd);

//             if (it == clients.end())
//                 continue;

//             ClientInfo &client = it->second;

//             // ------------------------------------
//             // 错误事件
//             // ------------------------------------

//             if (events[i].events &
//                 (EPOLLERR | EPOLLHUP))
//             {
//                 if (client.state != FINISHED &&
//                     client.state != FAILED)
//                 {
//                     client.state = FAILED;

//                     ++request_failed;
//                     ++finished_count;
//                 }

//                 continue;
//             }

//             // ------------------------------------
//             // CONNECTING
//             // ------------------------------------

//             if (client.state == CONNECTING &&
//                 (events[i].events & EPOLLOUT))
//             {
//                 if (!CheckConnect(fd))
//                 {
//                     client.state = FAILED;

//                     ++connect_failed;
//                     ++finished_count;

//                     continue;
//                 }

//                 ++connect_success;

//                 client.state = SENDING;
//             }

//             // ------------------------------------
//             // SENDING
//             // ------------------------------------

//             if (client.state == SENDING &&
//                 (events[i].events & EPOLLOUT))
//             {
//                 while (
//                     client.send_offset <
//                     client.send_buffer.size())
//                 {
//                     ssize_t ret = send(
//                         fd,
//                         client.send_buffer.data()
//                             + client.send_offset,
//                         client.send_buffer.size()
//                             - client.send_offset,
//                         0);

//                     if (ret > 0)
//                     {
//                         client.send_offset += ret;
//                     }
//                     else if (ret < 0)
//                     {
//                         if (errno == EINTR)
//                         {
//                             continue;
//                         }

//                         if (errno == EAGAIN ||
//                             errno == EWOULDBLOCK)
//                         {
//                             break;
//                         }

//                         client.state = FAILED;

//                         ++request_failed;
//                         ++finished_count;

//                         break;
//                     }
//                 }

//                 // 全部发送完成
//                 if (client.state == SENDING &&
//                     client.send_offset ==
//                         client.send_buffer.size())
//                 {
//                     client.state = RECEIVING;

//                     ModifyEvent(
//                         epfd,
//                         fd,
//                         EPOLLIN |
//                             EPOLLERR |
//                             EPOLLHUP);
//                 }
//             }

//             // ------------------------------------
//             // RECEIVING
//             // ------------------------------------

//             if (client.state == RECEIVING &&
//                 (events[i].events & EPOLLIN))
//             {
//                 char buffer[4096];

//                 while (true)
//                 {
//                     ssize_t ret = recv(
//                         fd,
//                         buffer,
//                         sizeof(buffer),
//                         0);

//                     if (ret > 0)
//                     {
//                         client.recv_buffer.append(
//                             buffer,
//                             ret);

//                         // Echo模式：
//                         // 发64字节，收满64字节即完成
//                         if (client.recv_buffer.size()
//                             >= PAYLOAD_SIZE)
//                         {
//                             client.state = FINISHED;

//                             ++request_success;
//                             ++finished_count;

//                             epoll_ctl(
//                                 epfd,
//                                 EPOLL_CTL_DEL,
//                                 fd,
//                                 nullptr);

//                             break;
//                         }
//                     }
//                     else if (ret == 0)
//                     {
//                         client.state = FAILED;

//                         ++request_failed;
//                         ++finished_count;

//                         break;
//                     }
//                     else
//                     {
//                         if (errno == EINTR)
//                             continue;

//                         if (errno == EAGAIN ||
//                             errno == EWOULDBLOCK)
//                         {
//                             break;
//                         }

//                         client.state = FAILED;

//                         ++request_failed;
//                         ++finished_count;

//                         break;
//                     }
//                 }
//             }
//         }
//     }

//     cout << endl;
//     cout << "========== Echo Stress Result =========="
//          << endl;

//     cout << "Target connections: "
//          << CONNECTIONS
//          << endl;

//     cout << "Connect success: "
//          << connect_success
//          << endl;

//     cout << "Connect failed: "
//          << connect_failed
//          << endl;

//     cout << "Echo success: "
//          << request_success
//          << endl;

//     cout << "Echo failed: "
//          << request_failed
//          << endl;

//     cout << "Payload size: "
//          << PAYLOAD_SIZE
//          << " bytes"
//          << endl;

//     cout << "========================================"
//          << endl;

//     for (auto &kv : clients)
//     {
//         if (kv.second.fd >= 0)
//             close(kv.second.fd);
//     }

//     close(epfd);

//     return 0;
// }

// // Part2 -- V2
// #include <iostream>
// #include <vector>
// #include <unordered_map>
// #include <cstring>
// #include <cerrno>
// #include <chrono>

// #include <unistd.h>
// #include <fcntl.h>
// #include <arpa/inet.h>
// #include <sys/socket.h>
// #include <sys/epoll.h>

// using namespace std;

// const char *SERVER_IP = "127.0.0.1";
// const int SERVER_PORT = 8080;

// const int CONNECTIONS = 29000; // 15000测试
// const int MAX_EVENTS = 4096;
// const int PAYLOAD_SIZE = 64;

// // epoll_wait单次等待时间
// const int EPOLL_TIMEOUT_MS = 1000;

// // 整个Echo测试最长允许时间
// const int TOTAL_TIMEOUT_SEC = 20;

// enum ClientState
// {
//     CONNECTING,
//     SENDING,
//     RECEIVING,
//     FINISHED,
//     FAILED
// };

// struct ClientInfo
// {
//     int fd = -1;

//     ClientState state = CONNECTING;

//     string send_buffer;
//     size_t send_offset = 0;

//     string recv_buffer;
// };

// // ==============================
// // 设置非阻塞
// // ==============================

// bool SetNonBlock(int fd)
// {
//     int flags = fcntl(fd, F_GETFL, 0);

//     if (flags < 0)
//     {
//         cout << "fcntl GET failed: "
//              << strerror(errno) << endl;

//         return false;
//     }

//     if (fcntl(fd,
//               F_SETFL,
//               flags | O_NONBLOCK) < 0)
//     {
//         cout << "fcntl SET failed: "
//              << strerror(errno) << endl;

//         return false;
//     }

//     return true;
// }

// // ==============================
// // 判断非阻塞connect最终结果
// // ==============================

// bool CheckConnect(int fd,
//                   int &socket_error)
// {
//     socket_error = 0;

//     socklen_t len = sizeof(socket_error);

//     if (getsockopt(fd,
//                    SOL_SOCKET,
//                    SO_ERROR,
//                    &socket_error,
//                    &len) < 0)
//     {
//         return false;
//     }

//     return socket_error == 0;
// }

// // ==============================
// // 修改epoll监听事件
// // ==============================

// bool ModifyEvent(int epfd,
//                  int fd,
//                  uint32_t events)
// {
//     epoll_event ev;

//     memset(&ev, 0, sizeof(ev));

//     ev.events = events;
//     ev.data.fd = fd;

//     if (epoll_ctl(epfd,
//                   EPOLL_CTL_MOD,
//                   fd,
//                   &ev) < 0)
//     {
//         cout << "epoll_ctl MOD failed fd:"
//              << fd
//              << " error:"
//              << strerror(errno)
//              << endl;

//         return false;
//     }

//     return true;
// }

// int main()
// {
//     int epfd = epoll_create1(0);

//     if (epfd < 0)
//     {
//         cout << "epoll_create1 failed: "
//              << strerror(errno)
//              << endl;

//         return 1;
//     }

//     sockaddr_in server_addr;

//     memset(&server_addr,
//            0,
//            sizeof(server_addr));

//     server_addr.sin_family = AF_INET;
//     server_addr.sin_port =
//         htons(SERVER_PORT);

//     if (inet_pton(AF_INET,
//                   SERVER_IP,
//                   &server_addr.sin_addr) <= 0)
//     {
//         cout << "inet_pton failed"
//              << endl;

//         return 1;
//     }

//     unordered_map<int, ClientInfo> clients;

//     // ==============================
//     // 各种统计
//     // ==============================

//     int connect_success = 0;
//     int connect_failed = 0;

//     int echo_success = 0;

//     int send_failed = 0;
//     int recv_failed = 0;

//     int peer_closed = 0;

//     int epoll_error_count = 0;
//     int epoll_hup_count = 0;

//     int timeout_unfinished = 0;

//     int finished_count = 0;

//     int reset_count = 0;
//     int refused_count = 0;
//     int timeout_count = 0;
//     int pipe_count = 0;
//     int other_socket_error = 0;

//     // ==============================
//     // 1. 创建连接
//     // ==============================

//     for (int i = 0;
//          i < CONNECTIONS;
//          ++i)
//     {
//         int fd = socket(AF_INET,
//                         SOCK_STREAM,
//                         0);

//         if (fd < 0)
//         {
//             ++connect_failed;
//             ++finished_count;
//             continue;
//         }

//         if (!SetNonBlock(fd))
//         {
//             close(fd);

//             ++connect_failed;
//             ++finished_count;

//             continue;
//         }

//         ClientInfo info;

//         info.fd = fd;
//         info.state = CONNECTING;

//         // 每个client发送64字节
//         info.send_buffer.assign(
//             PAYLOAD_SIZE,
//             'A');

//         clients.emplace(fd,
//                         std::move(info));

//         int ret = connect(
//             fd,
//             reinterpret_cast<sockaddr *>(
//                 &server_addr),
//             sizeof(server_addr));

//         if (ret == 0)
//         {
//             // connect立即成功
//             ++connect_success;

//             clients[fd].state = SENDING;

//             epoll_event ev;

//             memset(&ev,
//                    0,
//                    sizeof(ev));

//             ev.events =
//                 EPOLLOUT |
//                 EPOLLERR |
//                 EPOLLHUP;

//             ev.data.fd = fd;

//             if (epoll_ctl(epfd,
//                           EPOLL_CTL_ADD,
//                           fd,
//                           &ev) < 0)
//             {
//                 clients[fd].state = FAILED;

//                 ++connect_failed;
//                 ++finished_count;
//             }
//         }
//         else
//         {
//             if (errno == EINPROGRESS)
//             {
//                 epoll_event ev;

//                 memset(&ev,
//                        0,
//                        sizeof(ev));

//                 ev.events =
//                     EPOLLOUT |
//                     EPOLLERR |
//                     EPOLLHUP;

//                 ev.data.fd = fd;

//                 if (epoll_ctl(
//                         epfd,
//                         EPOLL_CTL_ADD,
//                         fd,
//                         &ev) < 0)
//                 {
//                     clients[fd].state =
//                         FAILED;

//                     ++connect_failed;
//                     ++finished_count;
//                 }
//             }
//             else
//             {
//                 // cout << "[CONNECT ERROR] fd="
//                 //      << fd
//                 //      << " "
//                 //      << strerror(errno)
//                 //      << endl;

//                 clients[fd].state =
//                     FAILED;

//                 ++connect_failed;
//                 ++finished_count;
//             }
//         }
//     }

//     // ==============================
//     // 2. 开始Echo测试
//     // ==============================

//     epoll_event events[MAX_EVENTS];

//     auto start_time =
//         chrono::steady_clock::now();

//     while (finished_count < CONNECTIONS)
//     {
//         // --------------------------
//         // 判断整个测试是否超过20秒
//         // --------------------------

//         auto now =
//             chrono::steady_clock::now();

//         auto elapsed =
//             chrono::duration_cast<
//                 chrono::seconds>(
//                 now - start_time)
//                 .count();

//         if (elapsed >=
//             TOTAL_TIMEOUT_SEC)
//         {
//             break;
//         }

//         int num = epoll_wait(
//             epfd,
//             events,
//             MAX_EVENTS,
//             EPOLL_TIMEOUT_MS);

//         if (num < 0)
//         {
//             if (errno == EINTR)
//                 continue;

//             // cout << "epoll_wait failed: "
//             //      << strerror(errno)
//             //      << endl;

//             break;
//         }

//         if (num == 0)
//         {
//             // 这里只代表这一秒没有事件
//             // 不是整个测试结束
//             continue;
//         }

//         // ==========================
//         // 处理活跃连接
//         // ==========================

//         for (int i = 0;
//              i < num;
//              ++i)
//         {
//             int fd =
//                 events[i].data.fd;

//             auto it =
//                 clients.find(fd);

//             if (it == clients.end())
//                 continue;

//             ClientInfo &client =
//                 it->second;

//             if (client.state ==
//                     FINISHED ||
//                 client.state ==
//                     FAILED)
//             {
//                 continue;
//             }

//             uint32_t revents =
//                 events[i].events;

//             // ==========================
//             // EPOLLERR
//             // ==========================

//             if (revents & EPOLLERR)
//             {
//                 int error = 0;
//                 socklen_t len = sizeof(error);

//                 int ret = getsockopt(
//                     fd,
//                     SOL_SOCKET,
//                     SO_ERROR,
//                     &error,
//                     &len);

//                 // cout << "[EPOLLERR]"
//                 //      << " fd=" << fd
//                 //      << " state=" << client.state
//                 //      << " sent=" << client.send_offset
//                 //      << "/" << client.send_buffer.size()
//                 //      << " recv=" << client.recv_buffer.size()
//                 //      << "/" << PAYLOAD_SIZE
//                 //      << " socket_error=" << error
//                 //      << " " << strerror(error)
//                 //      << endl;

//                 if (ret < 0)
//                 {
//                     cout << "[EPOLLERR] fd="
//                          << fd
//                          << " getsockopt failed: "
//                          << strerror(errno)
//                          << endl;

//                     ++other_socket_error;
//                 }
//                 else
//                 {
//                     // cout << "[EPOLLERR] fd="
//                     //      << fd
//                     //      << " socket_error="
//                     //      << error
//                     //      << " "
//                     //      << strerror(error)
//                     //      << endl;

//                     switch (error)
//                     {
//                     case ECONNRESET:
//                         ++reset_count;
//                         break;

//                     case ECONNREFUSED:
//                         ++refused_count;
//                         break;

//                     case ETIMEDOUT:
//                         ++timeout_count;
//                         break;

//                     case EPIPE:
//                         ++pipe_count;
//                         break;

//                     default:
//                         ++other_socket_error;
//                         break;
//                     }
//                 }

//                 client.state = FAILED;

//                 ++epoll_error_count;
//                 ++finished_count;

//                 epoll_ctl(
//                     epfd,
//                     EPOLL_CTL_DEL,
//                     fd,
//                     nullptr);

//                 continue;
//             }
//             // ==========================
//             // EPOLLHUP
//             // ==========================

//             if (revents & EPOLLHUP)
//             {
//                 cout << "[EPOLLHUP] fd="
//                      << fd
//                      << endl;

//                 client.state = FAILED;

//                 ++epoll_hup_count;
//                 ++finished_count;

//                 epoll_ctl(
//                     epfd,
//                     EPOLL_CTL_DEL,
//                     fd,
//                     nullptr);

//                 continue;
//             }

//             // ==========================
//             // CONNECTING
//             // ==========================

//             if (client.state ==
//                     CONNECTING &&
//                 (revents & EPOLLOUT))
//             {
//                 int error = 0;

//                 if (!CheckConnect(fd,
//                                   error))
//                 {
//                     // cout << "[CONNECT FAILED] fd="
//                     //      << fd
//                     //      << " error="
//                     //      << strerror(error)
//                     //      << endl;

//                     client.state = FAILED;

//                     ++connect_failed;
//                     ++finished_count;

//                     epoll_ctl(
//                         epfd,
//                         EPOLL_CTL_DEL,
//                         fd,
//                         nullptr);

//                     continue;
//                 }

//                 ++connect_success;

//                 client.state = SENDING;
//             }

//             // ==========================
//             // SENDING
//             // ==========================

//             if (client.state ==
//                     SENDING &&
//                 (revents & EPOLLOUT))
//             {
//                 while (
//                     client.send_offset <
//                     client.send_buffer.size())
//                 {
//                     ssize_t ret =
//                         send(
//                             fd,
//                             client.send_buffer.data() + client.send_offset,
//                             client.send_buffer.size() - client.send_offset,
//                             MSG_NOSIGNAL);

//                     if (ret > 0)
//                     {
//                         client.send_offset += ret;

//                         continue;
//                     }

//                     if (ret < 0)
//                     {
//                         if (errno == EINTR)
//                             continue;

//                         if (errno == EAGAIN ||
//                             errno == EWOULDBLOCK)
//                         {
//                             break;
//                         }

//                         // cout << "[SEND ERROR] fd="
//                         //      << fd
//                         //      << " error="
//                         //      << strerror(errno)
//                         //      << endl;

//                         client.state =
//                             FAILED;

//                         ++send_failed;
//                         ++finished_count;

//                         epoll_ctl(
//                             epfd,
//                             EPOLL_CTL_DEL,
//                             fd,
//                             nullptr);

//                         break;
//                     }
//                 }

//                 // ----------------------
//                 // 全部发送完成
//                 // ----------------------

//                 if (client.state ==
//                         SENDING &&
//                     client.send_offset ==
//                         client.send_buffer.size())
//                 {
//                     client.state =
//                         RECEIVING;

//                     ModifyEvent(
//                         epfd,
//                         fd,
//                         EPOLLIN |
//                             EPOLLERR |
//                             EPOLLHUP);
//                 }
//             }

//             // ==========================
//             // RECEIVING
//             // ==========================

//             if (client.state ==
//                     RECEIVING &&
//                 (revents & EPOLLIN))
//             {
//                 char buffer[4096];

//                 while (true)
//                 {
//                     ssize_t ret =
//                         recv(
//                             fd,
//                             buffer,
//                             sizeof(buffer),
//                             0);

//                     if (ret > 0)
//                     {
//                         client.recv_buffer
//                             .append(
//                                 buffer,
//                                 ret);

//                         // ------------------
//                         // 原样Echo：
//                         // 发64字节
//                         // 收满64字节
//                         // ------------------

//                         if (client.recv_buffer
//                                 .size() >=
//                             PAYLOAD_SIZE)
//                         {
//                             client.state =
//                                 FINISHED;

//                             ++echo_success;
//                             ++finished_count;

//                             epoll_ctl(
//                                 epfd,
//                                 EPOLL_CTL_DEL,
//                                 fd,
//                                 nullptr);

//                             break;
//                         }

//                         continue;
//                     }

//                     // ----------------------
//                     // 对端主动关闭
//                     // ----------------------

//                     if (ret == 0)
//                     {
//                         // cout << "[PEER CLOSED] fd="
//                         //      << fd
//                         //      << " recv="
//                         //      << client.recv_buffer.size()
//                         //      << "/"
//                         //      << PAYLOAD_SIZE
//                         //      << endl;

//                         client.state =
//                             FAILED;

//                         ++peer_closed;
//                         ++finished_count;

//                         epoll_ctl(
//                             epfd,
//                             EPOLL_CTL_DEL,
//                             fd,
//                             nullptr);

//                         break;
//                     }

//                     // ----------------------
//                     // recv < 0
//                     // ----------------------

//                     if (errno == EINTR)
//                         continue;

//                     if (errno == EAGAIN ||
//                         errno ==
//                             EWOULDBLOCK)
//                     {
//                         break;
//                     }

//                     // cout << "[RECV ERROR] fd="
//                     //      << fd
//                     //      << " error="
//                     //      << strerror(errno)
//                     //      << endl;

//                     client.state =
//                         FAILED;

//                     ++recv_failed;
//                     ++finished_count;

//                     epoll_ctl(
//                         epfd,
//                         EPOLL_CTL_DEL,
//                         fd,
//                         nullptr);

//                     break;
//                 }
//             }
//         }
//     }

//     // ==============================
//     // 3. 统计超时未完成连接
//     // ==============================

//     for (auto &kv : clients)
//     {
//         ClientInfo &client =
//             kv.second;

//         if (client.state !=
//                 FINISHED &&
//             client.state !=
//                 FAILED)
//         {
//             ++timeout_unfinished;

//             // cout << "[TIMEOUT] fd="
//             //      << client.fd
//             //      << " state="
//             //      << client.state
//             //      << " sent="
//             //      << client.send_offset
//             //      << "/"
//             //      << client.send_buffer.size()
//             //      << " recv="
//             //      << client.recv_buffer.size()
//             //      << "/"
//             //      << PAYLOAD_SIZE
//             //      << endl;

//             client.state = FAILED;
//         }
//     }

//     // ==============================
//     // 4. 输出最终测试结果
//     // ==============================

//     int echo_failed =
//         send_failed +
//         recv_failed +
//         peer_closed +
//         epoll_error_count +
//         epoll_hup_count +
//         timeout_unfinished;

//     cout << endl;

//     cout << endl;
//     cout << "========== Echo Stress Result ==========" << endl;

//     cout << "Target connections: "
//          << CONNECTIONS << endl;

//     cout << endl;

//     cout << "Connect success: "
//          << connect_success << endl;

//     cout << "Connect failed: "
//          << connect_failed << endl;

//     cout << endl;

//     cout << "Echo success: "
//          << echo_success << endl;

//     cout << "Echo failed: "
//          << echo_failed << endl;

//     cout << endl;

//     cout << "----- Failure Detail -----" << endl;

//     cout << "Send failed: "
//          << send_failed << endl;

//     cout << "Recv failed: "
//          << recv_failed << endl;

//     cout << "Peer closed: "
//          << peer_closed << endl;

//     cout << "EPOLLERR: "
//          << epoll_error_count << endl;

//     cout << "EPOLLHUP: "
//          << epoll_hup_count << endl;

//     cout << "Timeout unfinished: "
//          << timeout_unfinished << endl;

//     cout << endl;

//     cout << "----- EPOLLERR Detail -----" << endl;

//     cout << "ECONNRESET: "
//          << reset_count << endl;

//     cout << "ECONNREFUSED: "
//          << refused_count << endl;

//     cout << "ETIMEDOUT: "
//          << timeout_count << endl;

//     cout << "EPIPE: "
//          << pipe_count << endl;

//     cout << "Other socket errors: "
//          << other_socket_error << endl;

//     cout << "========================================" << endl;

//     // ==============================
//     // 5. 清理
//     // ==============================

//     for (auto &kv : clients)
//     {
//         if (kv.second.fd >= 0)
//         {
//             close(kv.second.fd);
//         }
//     }

//     close(epfd);

//     return 0;
// }

// // Part3
// #include <iostream>
// #include <vector>
// #include <unordered_map>
// #include <algorithm>
// #include <cstring>
// #include <cerrno>
// #include <chrono>

// #include <signal.h>
// #include <unistd.h>
// #include <fcntl.h>
// #include <arpa/inet.h>
// #include <sys/socket.h>
// #include <sys/epoll.h>

// using namespace std;

// const char *SERVER_IP = "127.0.0.1";
// const int SERVER_PORT = 8080;

// // 先从100开始
// const int CONNECTIONS = 5000;

// // 每次请求64字节
// const int PAYLOAD_SIZE = 64;

// // 正式QPS测试持续10秒
// const int TEST_DURATION_SEC = 10;

// const int MAX_EVENTS = 4096;

// // epoll每次最多等待100ms
// // 不要用它控制整个测试时间
// const int EPOLL_TIMEOUT_MS = 100;


// enum ClientState
// {
//     CONNECTING,
//     SENDING,
//     RECEIVING,
//     FAILED
// };


// struct ClientInfo
// {
//     int fd = -1;

//     ClientState state = CONNECTING;

//     string send_buffer;

//     size_t send_offset = 0;

//     // 当前这一轮已经收到多少字节
//     size_t recv_bytes = 0;

//     // 当前请求开始发送的时间
//     chrono::steady_clock::time_point request_start;
// };


// // =====================================
// // 设置非阻塞
// // =====================================

// bool SetNonBlock(int fd)
// {
//     int flags = fcntl(fd, F_GETFL, 0);

//     if (flags < 0)
//     {
//         return false;
//     }

//     if (fcntl(fd,
//               F_SETFL,
//               flags | O_NONBLOCK) < 0)
//     {
//         return false;
//     }

//     return true;
// }


// // =====================================
// // 检查非阻塞connect是否真正成功
// // =====================================

// bool CheckConnect(int fd)
// {
//     int error = 0;

//     socklen_t len = sizeof(error);

//     if (getsockopt(fd,
//                    SOL_SOCKET,
//                    SO_ERROR,
//                    &error,
//                    &len) < 0)
//     {
//         return false;
//     }

//     return error == 0;
// }


// // =====================================
// // 修改epoll监听事件
// // =====================================

// bool ModifyEvent(int epfd,
//                  int fd,
//                  uint32_t events)
// {
//     epoll_event ev;

//     memset(&ev,
//            0,
//            sizeof(ev));

//     ev.data.fd = fd;
//     ev.events = events;

//     if (epoll_ctl(epfd,
//                   EPOLL_CTL_MOD,
//                   fd,
//                   &ev) < 0)
//     {
//         return false;
//     }

//     return true;
// }


// // =====================================
// // 计算百分位延迟
// // 单位：微秒 us
// // =====================================

// double Percentile(
//     const vector<long long> &data,
//     double p)
// {
//     if (data.empty())
//     {
//         return 0.0;
//     }

//     size_t index =
//         static_cast<size_t>(
//             p * (data.size() - 1));

//     return static_cast<double>(
//         data[index]);
// }


// int main()
// {
//     signal(SIGPIPE, SIG_IGN);

//     int epfd = epoll_create1(0);

//     if (epfd < 0)
//     {
//         cout << "epoll_create1 failed: "
//              << strerror(errno)
//              << endl;

//         return 1;
//     }


//     sockaddr_in server_addr;

//     memset(&server_addr,
//            0,
//            sizeof(server_addr));

//     server_addr.sin_family = AF_INET;

//     server_addr.sin_port =
//         htons(SERVER_PORT);

//     if (inet_pton(AF_INET,
//                   SERVER_IP,
//                   &server_addr.sin_addr) <= 0)
//     {
//         cout << "inet_pton failed"
//              << endl;

//         return 1;
//     }


//     unordered_map<int, ClientInfo> clients;


//     int connect_success = 0;
//     int connect_failed = 0;

//     long long request_success = 0;

//     long long send_failed = 0;
//     long long recv_failed = 0;

//     long long epoll_error_count = 0;


//     // 保存每一次成功Echo的RTT
//     // 单位微秒
//     vector<long long> latencies;

//     // 如果QPS很高，这里可能很多
//     // 当前学习测试阶段先保留
//     latencies.reserve(1000000);


//     // =====================================
//     // 第一部分：创建所有连接
//     // =====================================

//     for (int i = 0;
//          i < CONNECTIONS;
//          ++i)
//     {
//         int fd = socket(
//             AF_INET,
//             SOCK_STREAM,
//             0);

//         if (fd < 0)
//         {
//             ++connect_failed;
//             continue;
//         }


//         if (!SetNonBlock(fd))
//         {
//             ++connect_failed;

//             close(fd);

//             continue;
//         }


//         ClientInfo info;

//         info.fd = fd;

//         info.state = CONNECTING;

//         info.send_buffer.assign(
//             PAYLOAD_SIZE,
//             'A');


//         clients.emplace(
//             fd,
//             std::move(info));


//         int ret = connect(
//             fd,
//             reinterpret_cast<sockaddr *>(
//                 &server_addr),
//             sizeof(server_addr));


//         epoll_event ev;

//         memset(&ev,
//                0,
//                sizeof(ev));

//         ev.data.fd = fd;

//         ev.events =
//             EPOLLOUT |
//             EPOLLERR |
//             EPOLLHUP;


//         if (ret == 0)
//         {
//             ++connect_success;

//             clients[fd].state =
//                 SENDING;


//             if (epoll_ctl(
//                     epfd,
//                     EPOLL_CTL_ADD,
//                     fd,
//                     &ev) < 0)
//             {
//                 ++connect_failed;

//                 clients[fd].state =
//                     FAILED;
//             }
//         }
//         else if (errno == EINPROGRESS)
//         {
//             if (epoll_ctl(
//                     epfd,
//                     EPOLL_CTL_ADD,
//                     fd,
//                     &ev) < 0)
//             {
//                 ++connect_failed;

//                 clients[fd].state =
//                     FAILED;
//             }
//         }
//         else
//         {
//             ++connect_failed;

//             clients[fd].state =
//                 FAILED;
//         }
//     }


//     // =====================================
//     // 先等所有连接完成
//     // 这一段不计入正式QPS测试时间
//     // =====================================

//     epoll_event events[MAX_EVENTS];


//     while (connect_success + connect_failed
//            < CONNECTIONS)
//     {
//         int num = epoll_wait(
//             epfd,
//             events,
//             MAX_EVENTS,
//             1000);


//         if (num < 0)
//         {
//             if (errno == EINTR)
//             {
//                 continue;
//             }

//             cout << "epoll_wait failed"
//                  << endl;

//             break;
//         }


//         if (num == 0)
//         {
//             continue;
//         }


//         for (int i = 0;
//              i < num;
//              ++i)
//         {
//             int fd =
//                 events[i].data.fd;


//             auto it =
//                 clients.find(fd);


//             if (it == clients.end())
//             {
//                 continue;
//             }


//             ClientInfo &client =
//                 it->second;


//             if (client.state !=
//                 CONNECTING)
//             {
//                 continue;
//             }


//             if (events[i].events &
//                 (EPOLLERR | EPOLLHUP))
//             {
//                 ++connect_failed;

//                 client.state =
//                     FAILED;

//                 continue;
//             }


//             if (events[i].events &
//                 EPOLLOUT)
//             {
//                 if (CheckConnect(fd))
//                 {
//                     ++connect_success;

//                     client.state =
//                         SENDING;
//                 }
//                 else
//                 {
//                     ++connect_failed;

//                     client.state =
//                         FAILED;
//                 }
//             }
//         }
//     }


//     cout << "Connections ready: "
//          << connect_success
//          << "/"
//          << CONNECTIONS
//          << endl;


//     // =====================================
//     // 正式测试开始
//     // =====================================

//     auto test_start =
//         chrono::steady_clock::now();

//     auto test_end =
//         test_start +
//         chrono::seconds(
//             TEST_DURATION_SEC);


//     // 已连接的client一开始都准备发送
//     for (auto &kv : clients)
//     {
//         ClientInfo &client =
//             kv.second;

//         if (client.state ==
//             SENDING)
//         {
//             client.send_offset = 0;

//             client.recv_bytes = 0;

//             client.request_start =
//                 chrono::steady_clock::now();

//             ModifyEvent(
//                 epfd,
//                 client.fd,
//                 EPOLLOUT |
//                 EPOLLERR |
//                 EPOLLHUP);
//         }
//     }


//     // =====================================
//     // 10秒持续Echo
//     // =====================================

//     while (chrono::steady_clock::now()
//            < test_end)
//     {
//         int num = epoll_wait(
//             epfd,
//             events,
//             MAX_EVENTS,
//             EPOLL_TIMEOUT_MS);


//         if (num < 0)
//         {
//             if (errno == EINTR)
//             {
//                 continue;
//             }

//             cout << "epoll_wait fatal: "
//                  << strerror(errno)
//                  << endl;

//             break;
//         }


//         if (num == 0)
//         {
//             continue;
//         }


//         for (int i = 0;
//              i < num;
//              ++i)
//         {
//             int fd =
//                 events[i].data.fd;


//             auto it =
//                 clients.find(fd);


//             if (it == clients.end())
//             {
//                 continue;
//             }


//             ClientInfo &client =
//                 it->second;


//             if (client.state ==
//                 FAILED)
//             {
//                 continue;
//             }


//             uint32_t revents =
//                 events[i].events;


//             // =============================
//             // socket错误
//             // =============================

//             if (revents &
//                 (EPOLLERR | EPOLLHUP))
//             {
//                 ++epoll_error_count;

//                 client.state =
//                     FAILED;

//                 epoll_ctl(
//                     epfd,
//                     EPOLL_CTL_DEL,
//                     fd,
//                     nullptr);

//                 continue;
//             }


//             // =============================
//             // SENDING
//             // =============================

//             if (client.state ==
//                     SENDING &&
//                 (revents & EPOLLOUT))
//             {
//                 while (
//                     client.send_offset
//                     <
//                     client.send_buffer.size())
//                 {
//                     ssize_t ret =
//                         send(
//                             fd,
//                             client.send_buffer.data()
//                                 +
//                                 client.send_offset,
//                             client.send_buffer.size()
//                                 -
//                                 client.send_offset,
//                             MSG_NOSIGNAL);


//                     if (ret > 0)
//                     {
//                         client.send_offset
//                             += ret;

//                         continue;
//                     }


//                     if (ret < 0)
//                     {
//                         if (errno ==
//                             EINTR)
//                         {
//                             continue;
//                         }


//                         if (errno ==
//                                 EAGAIN ||
//                             errno ==
//                                 EWOULDBLOCK)
//                         {
//                             break;
//                         }


//                         ++send_failed;

//                         client.state =
//                             FAILED;

//                         epoll_ctl(
//                             epfd,
//                             EPOLL_CTL_DEL,
//                             fd,
//                             nullptr);

//                         break;
//                     }
//                 }


//                 // 一条请求全部发送完成
//                 if (client.state ==
//                         SENDING &&
//                     client.send_offset ==
//                         client.send_buffer
//                             .size())
//                 {
//                     client.state =
//                         RECEIVING;

//                     client.recv_bytes = 0;


//                     ModifyEvent(
//                         epfd,
//                         fd,
//                         EPOLLIN |
//                             EPOLLERR |
//                             EPOLLHUP);
//                 }
//             }


//             // =============================
//             // RECEIVING
//             // =============================

//             if (client.state ==
//                     RECEIVING &&
//                 (revents & EPOLLIN))
//             {
//                 char buffer[4096];


//                 while (true)
//                 {
//                     ssize_t ret =
//                         recv(
//                             fd,
//                             buffer,
//                             sizeof(buffer),
//                             0);


//                     if (ret > 0)
//                     {
//                         client.recv_bytes
//                             += ret;


//                         // 收满64B
//                         // 一次Echo请求完成
//                         if (client.recv_bytes
//                             >= PAYLOAD_SIZE)
//                         {
//                             auto finish =
//                                 chrono::steady_clock::now();


//                             auto latency =
//                                 chrono::duration_cast<
//                                     chrono::microseconds>(
//                                     finish -
//                                     client.request_start)
//                                     .count();


//                             latencies.push_back(
//                                 latency);


//                             ++request_success;


//                             // =====================
//                             // 准备下一条请求
//                             // =====================

//                             client.state =
//                                 SENDING;

//                             client.send_offset =
//                                 0;

//                             client.recv_bytes =
//                                 0;


//                             client.request_start =
//                                 chrono::steady_clock::now();


//                             ModifyEvent(
//                                 epfd,
//                                 fd,
//                                 EPOLLOUT |
//                                     EPOLLERR |
//                                     EPOLLHUP);


//                             break;
//                         }


//                         continue;
//                     }


//                     if (ret == 0)
//                     {
//                         ++recv_failed;

//                         client.state =
//                             FAILED;

//                         epoll_ctl(
//                             epfd,
//                             EPOLL_CTL_DEL,
//                             fd,
//                             nullptr);

//                         break;
//                     }


//                     if (errno ==
//                         EINTR)
//                     {
//                         continue;
//                     }


//                     if (errno ==
//                             EAGAIN ||
//                         errno ==
//                             EWOULDBLOCK)
//                     {
//                         break;
//                     }


//                     ++recv_failed;

//                     client.state =
//                         FAILED;


//                     epoll_ctl(
//                         epfd,
//                         EPOLL_CTL_DEL,
//                         fd,
//                         nullptr);


//                     break;
//                 }
//             }
//         }
//     }


//     // =====================================
//     // 正式测试结束时间
//     // =====================================

//     auto real_end =
//         chrono::steady_clock::now();


//     double duration =
//         chrono::duration_cast<
//             chrono::duration<double>>(
//             real_end -
//             test_start)
//             .count();


//     // =====================================
//     // 延迟统计
//     // =====================================

//     sort(
//         latencies.begin(),
//         latencies.end());


//     double avg_latency = 0;


//     if (!latencies.empty())
//     {
//         long long total_latency = 0;

//         for (long long value :
//              latencies)
//         {
//             total_latency += value;
//         }


//         avg_latency =
//             static_cast<double>(
//                 total_latency)
//             /
//             latencies.size();
//     }


//     double p50 =
//         Percentile(
//             latencies,
//             0.50);


//     double p95 =
//         Percentile(
//             latencies,
//             0.95);


//     double p99 =
//         Percentile(
//             latencies,
//             0.99);


//     double qps = 0;


//     if (duration > 0)
//     {
//         qps =
//             static_cast<double>(
//                 request_success)
//             /
//             duration;
//     }


//     // =====================================
//     // 输出结果
//     // =====================================

//     cout << endl;

//     cout << "========== QPS Stress Result =========="
//          << endl;


//     cout << "Connections: "
//          << CONNECTIONS
//          << endl;


//     cout << "Connect success: "
//          << connect_success
//          << endl;


//     cout << "Connect failed: "
//          << connect_failed
//          << endl;


//     cout << endl;


//     cout << "Payload: "
//          << PAYLOAD_SIZE
//          << " bytes"
//          << endl;


//     cout << "Duration: "
//          << duration
//          << " sec"
//          << endl;


//     cout << endl;


//     cout << "Successful requests: "
//          << request_success
//          << endl;


//     cout << "QPS: "
//          << qps
//          << endl;


//     cout << endl;


//     cout << "Average latency: "
//          << avg_latency
//          << " us"
//          << endl;


//     cout << "P50 latency: "
//          << p50
//          << " us"
//          << endl;


//     cout << "P95 latency: "
//          << p95
//          << " us"
//          << endl;


//     cout << "P99 latency: "
//          << p99
//          << " us"
//          << endl;


//     cout << endl;


//     cout << "Send failed: "
//          << send_failed
//          << endl;


//     cout << "Recv failed: "
//          << recv_failed
//          << endl;


//     cout << "Epoll errors: "
//          << epoll_error_count
//          << endl;


//     cout << "======================================="
//          << endl;


//     // =====================================
//     // 清理
//     // =====================================

//     for (auto &kv : clients)
//     {
//         if (kv.second.fd >= 0)
//         {
//             close(kv.second.fd);
//         }
//     }


//     close(epfd);

//     return 0;
// }

// Part4
#include <iostream>
#include <unordered_map>
#include <cstring>
#include <cerrno>
#include <chrono>

#include <signal.h>
#include <unistd.h>
#include <fcntl.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <sys/epoll.h>

using namespace std;

const char *SERVER_IP = "127.0.0.1";
const int SERVER_PORT = 8080;

// 第一轮先测单连接
const int CONNECTIONS = 1;

// 一次发送64KB
const size_t PAYLOAD_SIZE = 1024 * 1024;

// 正式测试10秒
const int TEST_DURATION_SEC = 10;

const int MAX_EVENTS = 4096;
const int EPOLL_TIMEOUT_MS = 100;

enum ClientState
{
    CONNECTING,
    SENDING,
    RECEIVING,
    FAILED
};

struct ClientInfo
{
    int fd = -1;

    ClientState state = CONNECTING;

    string send_buffer;

    // 当前这一轮已经send多少
    size_t send_offset = 0;

    // 当前这一轮已经recv多少
    size_t recv_bytes = 0;
};

// ===================================================
// 设置非阻塞
// ===================================================

bool SetNonBlock(int fd)
{
    int flags = fcntl(fd, F_GETFL, 0);

    if (flags < 0)
        return false;

    if (fcntl(fd,
              F_SETFL,
              flags | O_NONBLOCK) < 0)
    {
        return false;
    }

    return true;
}

// ===================================================
// 检查非阻塞connect结果
// ===================================================

bool CheckConnect(int fd)
{
    int error = 0;

    socklen_t len = sizeof(error);

    if (getsockopt(fd,
                   SOL_SOCKET,
                   SO_ERROR,
                   &error,
                   &len) < 0)
    {
        return false;
    }

    return error == 0;
}

// ===================================================
// 修改epoll监听事件
// ===================================================

bool ModifyEvent(int epfd,
                 int fd,
                 uint32_t events)
{
    epoll_event ev;

    memset(&ev,
           0,
           sizeof(ev));

    ev.data.fd = fd;
    ev.events = events;

    if (epoll_ctl(epfd,
                  EPOLL_CTL_MOD,
                  fd,
                  &ev) < 0)
    {
        return false;
    }

    return true;
}

int main()
{
    signal(SIGPIPE, SIG_IGN);

    int epfd = epoll_create1(0);

    if (epfd < 0)
    {
        cout << "epoll_create1 failed: "
             << strerror(errno)
             << endl;

        return 1;
    }

    sockaddr_in server_addr;

    memset(&server_addr,
           0,
           sizeof(server_addr));

    server_addr.sin_family = AF_INET;

    server_addr.sin_port =
        htons(SERVER_PORT);

    if (inet_pton(AF_INET,
                  SERVER_IP,
                  &server_addr.sin_addr) <= 0)
    {
        cout << "inet_pton failed"
             << endl;

        return 1;
    }

    unordered_map<int, ClientInfo> clients;

    int connect_success = 0;
    int connect_failed = 0;

    long long send_failed = 0;
    long long recv_failed = 0;
    long long epoll_errors = 0;

    // ================================
    // 整个测试期间累计成功发送/接收字节数
    // ================================

    unsigned long long total_sent_bytes = 0;
    unsigned long long total_recv_bytes = 0;

    // 完整完成了多少轮Echo
    unsigned long long completed_rounds = 0;

    // ===================================================
    // 1. 建立所有连接
    // ===================================================

    for (int i = 0;
         i < CONNECTIONS;
         ++i)
    {
        int fd = socket(
            AF_INET,
            SOCK_STREAM,
            0);

        if (fd < 0)
        {
            ++connect_failed;
            continue;
        }

        if (!SetNonBlock(fd))
        {
            ++connect_failed;

            close(fd);

            continue;
        }

        ClientInfo info;

        info.fd = fd;

        info.state = CONNECTING;

        // 生成PAYLOAD_SIZE字节测试数据
        info.send_buffer.assign(
            PAYLOAD_SIZE,
            'A');

        clients.emplace(
            fd,
            std::move(info));

        int ret = connect(
            fd,
            reinterpret_cast<sockaddr *>(
                &server_addr),
            sizeof(server_addr));

        epoll_event ev;

        memset(&ev,
               0,
               sizeof(ev));

        ev.data.fd = fd;

        ev.events =
            EPOLLOUT |
            EPOLLERR |
            EPOLLHUP;

        if (ret == 0)
        {
            ++connect_success;

            clients[fd].state =
                SENDING;

            if (epoll_ctl(
                    epfd,
                    EPOLL_CTL_ADD,
                    fd,
                    &ev) < 0)
            {
                ++connect_failed;

                clients[fd].state =
                    FAILED;
            }
        }
        else if (errno == EINPROGRESS)
        {
            if (epoll_ctl(
                    epfd,
                    EPOLL_CTL_ADD,
                    fd,
                    &ev) < 0)
            {
                ++connect_failed;

                clients[fd].state =
                    FAILED;
            }
        }
        else
        {
            ++connect_failed;

            clients[fd].state =
                FAILED;
        }
    }

    epoll_event events[MAX_EVENTS];

    // ===================================================
    // 2. 等待所有connect完成
    // 不算入正式吞吐测试时间
    // ===================================================

    while (connect_success + connect_failed
           < CONNECTIONS)
    {
        int num = epoll_wait(
            epfd,
            events,
            MAX_EVENTS,
            1000);

        if (num < 0)
        {
            if (errno == EINTR)
                continue;

            cout << "epoll_wait failed"
                 << endl;

            break;
        }

        if (num == 0)
            continue;

        for (int i = 0;
             i < num;
             ++i)
        {
            int fd =
                events[i].data.fd;

            auto it =
                clients.find(fd);

            if (it == clients.end())
                continue;

            ClientInfo &client =
                it->second;

            if (client.state !=
                CONNECTING)
            {
                continue;
            }

            if (events[i].events &
                (EPOLLERR | EPOLLHUP))
            {
                ++connect_failed;

                client.state =
                    FAILED;

                continue;
            }

            if (events[i].events &
                EPOLLOUT)
            {
                if (CheckConnect(fd))
                {
                    ++connect_success;

                    client.state =
                        SENDING;
                }
                else
                {
                    ++connect_failed;

                    client.state =
                        FAILED;
                }
            }
        }
    }

    cout << "Connections ready: "
         << connect_success
         << "/"
         << CONNECTIONS
         << endl;

    // ===================================================
    // 3. 初始化正式测试
    // ===================================================

    for (auto &kv : clients)
    {
        ClientInfo &client =
            kv.second;

        if (client.state ==
            SENDING)
        {
            client.send_offset = 0;
            client.recv_bytes = 0;

            ModifyEvent(
                epfd,
                client.fd,
                EPOLLOUT |
                    EPOLLERR |
                    EPOLLHUP);
        }
    }

    auto test_start =
        chrono::steady_clock::now();

    auto test_end =
        test_start +
        chrono::seconds(
            TEST_DURATION_SEC);

    // ===================================================
    // 4. 正式吞吐测试
    // ===================================================

    while (chrono::steady_clock::now()
           < test_end)
    {
        int num = epoll_wait(
            epfd,
            events,
            MAX_EVENTS,
            EPOLL_TIMEOUT_MS);

        if (num < 0)
        {
            if (errno == EINTR)
                continue;

            cout << "epoll_wait failed: "
                 << strerror(errno)
                 << endl;

            break;
        }

        if (num == 0)
            continue;

        for (int i = 0;
             i < num;
             ++i)
        {
            int fd =
                events[i].data.fd;

            auto it =
                clients.find(fd);

            if (it == clients.end())
                continue;

            ClientInfo &client =
                it->second;

            if (client.state ==
                FAILED)
            {
                continue;
            }

            uint32_t revents =
                events[i].events;

            // ================================
            // socket错误
            // ================================

            if (revents &
                (EPOLLERR | EPOLLHUP))
            {
                ++epoll_errors;

                client.state =
                    FAILED;

                epoll_ctl(
                    epfd,
                    EPOLL_CTL_DEL,
                    fd,
                    nullptr);

                continue;
            }

            // ================================
            // SENDING
            // ================================

            if (client.state ==
                    SENDING &&
                (revents & EPOLLOUT))
            {
                while (
                    client.send_offset <
                    PAYLOAD_SIZE)
                {
                    ssize_t ret =
                        send(
                            fd,
                            client.send_buffer.data()
                                +
                                client.send_offset,
                            PAYLOAD_SIZE
                                -
                                client.send_offset,
                            MSG_NOSIGNAL);

                    if (ret > 0)
                    {
                        client.send_offset
                            += ret;

                        // 注意：
                        // 只统计真正成功交给kernel的字节
                        total_sent_bytes
                            += ret;

                        continue;
                    }

                    if (ret < 0)
                    {
                        if (errno ==
                            EINTR)
                        {
                            continue;
                        }

                        if (errno ==
                                EAGAIN ||
                            errno ==
                                EWOULDBLOCK)
                        {
                            break;
                        }

                        ++send_failed;

                        client.state =
                            FAILED;

                        epoll_ctl(
                            epfd,
                            EPOLL_CTL_DEL,
                            fd,
                            nullptr);

                        break;
                    }
                }

                // 当前一轮Payload全部发送完成
                if (client.state ==
                        SENDING &&
                    client.send_offset ==
                        PAYLOAD_SIZE)
                {
                    client.state =
                        RECEIVING;

                    client.recv_bytes = 0;

                    ModifyEvent(
                        epfd,
                        fd,
                        EPOLLIN |
                            EPOLLERR |
                            EPOLLHUP);
                }
            }

            // ================================
            // RECEIVING
            // ================================

            if (client.state ==
                    RECEIVING &&
                (revents & EPOLLIN))
            {
                char buffer[64 * 1024];

                while (true)
                {
                    ssize_t ret =
                        recv(
                            fd,
                            buffer,
                            sizeof(buffer),
                            0);

                    if (ret > 0)
                    {
                        client.recv_bytes
                            += ret;

                        total_recv_bytes
                            += ret;

                        // 收完整一轮Payload
                        if (client.recv_bytes
                            >= PAYLOAD_SIZE)
                        {
                            ++completed_rounds;

                            // ========================
                            // 下一轮重新发送64KB
                            // ========================

                            client.state =
                                SENDING;

                            client.send_offset = 0;
                            client.recv_bytes = 0;

                            ModifyEvent(
                                epfd,
                                fd,
                                EPOLLOUT |
                                    EPOLLERR |
                                    EPOLLHUP);

                            break;
                        }

                        continue;
                    }

                    if (ret == 0)
                    {
                        ++recv_failed;

                        client.state =
                            FAILED;

                        epoll_ctl(
                            epfd,
                            EPOLL_CTL_DEL,
                            fd,
                            nullptr);

                        break;
                    }

                    if (errno ==
                        EINTR)
                    {
                        continue;
                    }

                    if (errno ==
                            EAGAIN ||
                        errno ==
                            EWOULDBLOCK)
                    {
                        break;
                    }

                    ++recv_failed;

                    client.state =
                        FAILED;

                    epoll_ctl(
                        epfd,
                        EPOLL_CTL_DEL,
                        fd,
                        nullptr);

                    break;
                }
            }
        }
    }

    auto real_end =
        chrono::steady_clock::now();

    double duration =
        chrono::duration_cast<
            chrono::duration<double>>(
            real_end -
            test_start)
            .count();

    // ===================================================
    // 5. 计算吞吐
    // ===================================================

    const double MB =
        1024.0 * 1024.0;

    double sent_mb =
        total_sent_bytes / MB;

    double recv_mb =
        total_recv_bytes / MB;

    double send_throughput =
        sent_mb / duration;

    double recv_throughput =
        recv_mb / duration;

    // Echo场景下：
    // client->server + server->client
    double aggregate_throughput =
        (sent_mb + recv_mb)
        /
        duration;

    // ===================================================
    // 6. 输出结果
    // ===================================================

    cout << endl;

    cout << "========== Throughput Stress Result =========="
         << endl;

    cout << "Connections: "
         << CONNECTIONS
         << endl;

    cout << "Connect success: "
         << connect_success
         << endl;

    cout << "Connect failed: "
         << connect_failed
         << endl;

    cout << endl;

    cout << "Payload size: "
         << PAYLOAD_SIZE
         << " bytes"
         << endl;

    cout << "Payload size: "
         << PAYLOAD_SIZE / 1024.0
         << " KB"
         << endl;

    cout << "Duration: "
         << duration
         << " sec"
         << endl;

    cout << endl;

    cout << "Completed Echo rounds: "
         << completed_rounds
         << endl;

    cout << endl;

    cout << "Total sent: "
         << sent_mb
         << " MB"
         << endl;

    cout << "Total received: "
         << recv_mb
         << " MB"
         << endl;

    cout << endl;

    cout << "Send throughput: "
         << send_throughput
         << " MB/s"
         << endl;

    cout << "Receive throughput: "
         << recv_throughput
         << " MB/s"
         << endl;

    cout << "Bidirectional aggregate throughput: "
         << aggregate_throughput
         << " MB/s"
         << endl;

    cout << endl;

    cout << "Send failed: "
         << send_failed
         << endl;

    cout << "Recv failed: "
         << recv_failed
         << endl;

    cout << "Epoll errors: "
         << epoll_errors
         << endl;

    cout << "=============================================="
         << endl;

    // ===================================================
    // 7. 清理
    // ===================================================

    for (auto &kv : clients)
    {
        if (kv.second.fd >= 0)
        {
            close(kv.second.fd);
        }
    }

    close(epfd);

    return 0;
}