# MuduoServer V1：从 Socket 到单 Reactor 的完整学习与踩坑复盘

> 面向零基础读者的项目说明、原理教程、错误手册与验收指南。
>
> **最终定位：Linux / C++ 单 Reactor、单 EventLoop 的 TCP 网络服务器框架第一版。它不是完整 muduo，也不是已经实现的主从 Reactor。**
>
> 整理日期：2026-08-31。

## 阅读说明与事实边界

这份文档解释的不只是“代码怎么写”，还包括：为什么从一个简单 TCP 程序逐步拆出这些类，为什么看似能运行的代码会在断连或大数据时出错，以及如何证明改动有效。

资料范围说明：初读建议：先读第 1～7 章建立整体图景，再读第 8～12 章理解 V1 的关键难点，最后按第 13～15 章复盘、测试和准备面试。

## 目录

1. [项目目标、完成范围与学习路线](#s1)
2. [零基础必懂：进程、线程、fd、字节与 TCP](#s2)
3. [阶段一、二：Socket 与 TCP 多连接](#s3)
4. [阶段三：线程、阻塞队列与线程池探索](#s4)
5. [阶段四、五：epoll、非阻塞与 Reactor 思维](#s5)
6. [阶段六：V1 最终架构与七个核心类](#s6)
7. [回调、初始化与一轮事件循环](#s7)
8. [阶段七 7.1：Channel 生命周期](#s8)
9. [阶段七 7.2：TcpConnection 与延迟销毁](#s9)
10. [阶段七 7.3：Buffer 与真实 NUL 字节](#s10)
11. [阶段七 7.4：非阻塞读写与 EPOLLOUT](#s11)
12. [阶段七 7.5、7.6：data.ptr 与 eventfd 退出](#s12)
13. [整个学习过程的错误与改正手册](#s13)
14. [测试方法、日志证据与验收标准](#s14)
15. [面试讲述与自测题](#s15)
16. [可用于 README 的项目总结](#s16)
17. [术语速查与参考资料](#s17)

---

<a id="s1"></a>
## 1. 项目目标、完成范围与学习路线

### 1.1 这个项目到底解决什么问题

最简单的服务器可以接收一个客户端并回复一段文字。但真实的连接不会整齐地“来一个、说完、走一个”：有人连接后不说话，有人一次发几 MB，有人接收得很慢，有人突然断开。

本项目要解决的是：**用一个事件循环管理多个连接，让暂时不能读写的连接交还执行机会，并使数据和对象在正确的时机保存、消费、释放。**

这里的“高性能”首先描述技术方向：I/O 多路复用、非阻塞、事件驱动。没有正式压测数据时，不应据此宣称具体 QPS、百万连接或生产级性能。

### 1.2 学习路线

```text
Linux / Socket 基础（包含 UDP 对照学习）
                  |
                  v
TCP：建立连接 -> recv/send -> 多客户端
                  |
                  v
线程版：一个连接交给一个线程
                  |
                  v
线程池版：固定工作线程 + 任务队列
                  |
                  v
epoll 版：统一等待多个 fd 的就绪事件
                  |
                  v
非阻塞版：accept/recv/send 不等待单个连接
                  |
                  v
Reactor 框架：Channel / Epoller / EventLoop 等职责拆分
                  |
                  v
V1 收尾：状态机 + 延迟销毁 + Buffer + I/O 边界 + eventfd
```

阶段一到五是学习与演进路线，不表示最终服务器同时保留并运行全部旧模型。原对话中的“阶段六”是 Reactor 框架，“阶段七”是工程化收尾；本文件保留这一对应。

### 1.3 V1 完成内容与未完成内容

| 范围 | V1 定位 |
|---|---|
| 单 Reactor / 单 EventLoop | 最终网络 I/O 模型 |
| 多客户端 TCP 通信 | 已实现并有历史测试记录 |
| Acceptor、Channel、Epoller、EventLoop、TcpConnection、TcpServer | 已形成职责划分 |
| 输入输出 Buffer、按长度处理字节 | V1 收尾内容 |
| Channel 状态机、连接逻辑关闭、延迟销毁 | V1 收尾内容 |
| 非阻塞读写、部分发送、动态 EPOLLOUT | 已实现并有大数据测试讨论 |
| data.ptr 直接定位 Channel | 已完成优化；不等于智能指针改造 |
| eventfd、atomic<bool>、跨线程 Quit | 已完成最小唤醒退出测试 |
| EventLoopThread / EventLoopThreadPool / 主从 Reactor | 后续扩展，不记为 V1 功能 |
| 通用跨线程任务队列、runInLoop / queueInLoop | 不记为 V1 功能 |
| Timer、完整日志系统、HTTP / RPC 协议 | 不记为 V1 功能 |
| 通用拆包、完整半关闭与优雅排空发送 | 不能仅凭现有记录宣称已完成 |
| 生产部署、高负载容量与正式性能结论 | 本文没有提供此类验证 |

---

<a id="s2"></a>
## 2. 零基础必懂：进程、线程、fd、字节与 TCP

### 2.1 进程与线程

进程可以先理解为“正在运行的程序及其资源”。服务器启动后是一个进程。线程是进程内执行代码的一条路径，同一进程的线程通常共享内存。

共享内存既方便又危险：两个线程访问同一个变量，只要其中一个写入而又没有恰当同步，就可能发生数据竞争。后面的 `atomic<bool>` 正是为特定共享状态提供同步。

**并发**表示多个任务在一段时间内都能推进；**并行**表示同一时刻确实同时执行。单线程 Reactor 可以并发处理多连接，不代表同时用多个 CPU 核心执行这些连接的回调。

### 2.2 fd 是什么

fd 是 file descriptor，即文件描述符。对程序来说，它通常表现为一个非负整数，用于向内核指定某个已打开的资源。

```text
listenfd : 监听 socket，用来接受新连接
clientfd : 已连接 socket，用来收发某个客户端的数据
epfd     : epoll 实例
wakefd   : eventfd 通知资源
```

这些都是整数，但职责不同。fd 不是对象，也不保存 C++ 回调。fd 被关闭后可能很快被复用，所以 fd 数字不一定能唯一标识一条连接的一生。

`0` 也是合法 fd；判断创建成功应使用 `fd >= 0`，不是 `fd > 0`。

### 2.3 用户态与内核态

可以把用户态理解为“你的 C++ 对象和业务代码所在的一侧”，内核态理解为“操作系统管理 TCP、缓冲区和就绪事件的一侧”。

```text
用户态                                       内核态
Buffer -- send --> socket 发送缓冲区 -- TCP --> 网络
Buffer <-- recv -- socket 接收缓冲区 <-- TCP -- 网络
Channel -- epoll_ctl --> epoll 监听记录
EventLoop <-- epoll_wait -- 就绪事件
```

修改 `Channel::events_` 只修改用户态内存。必须通过 `epoll_ctl` 才能把监听需求同步给内核。

### 2.4 TCP 是字节流

TCP 提供有序的字节流传输，不保留应用调用 `send()` 时的分块边界。

```text
发送方：send("hello") + send("world")

接收方可能：
recv -> "helloworld"

也可能：
recv -> "he"
recv -> "llowor"
recv -> "ld"
```

它们表示相同的十个有序字节。这不是 TCP 把内容弄乱了，而是应用不能把一次接收当作一条消息。

“半包”“粘包”是应用层对消息边界的描述。Buffer 保存数据；协议负责规定哪些字节组成完整消息。仅有 Buffer 不会自动解决消息边界。

### 2.5 字节、字符与 NUL

一个字节是 8 位。字符只是字节的一种解释方式，中文在 UTF-8 中通常占多个字节。网络 API 返回的长度是字节数，不是屏幕上的汉字数量。

`NUL` 是值为 `0x00` 的字节，C++ 写法为 `'\0'`。它不同于空指针 `nullptr`，也不同于键盘输入的反斜杠加数字零 `\0`。在网络数据里，NUL 可以是有效内容。

---

<a id="s3"></a>
## 3. 阶段一、二：Socket 与 TCP 多连接

### 3.1 建立连接的完整流程

```text
服务器                                      客户端
socket()                                    socket()
   |
bind(IP, port)
   |
listen()
   |                                        connect(IP, port)
   | <---------- TCP 建立连接 ------------------ |
accept() -> clientfd
   |                                        send()
recv() <-------------------------------------- |
业务处理
send() -------------------------------------> recv()
   |                                            |
close(clientfd)                              close(fd)

listenfd 保留，可继续 accept 其他连接
```

TCP 握手由内核协议栈推进；`accept()` 从监听 socket 的待接受连接中取出一个连接，返回新的已连接 fd。它不是读取应用业务数据的函数。[Linux accept(2)](https://man7.org/linux/man-pages/man2/accept.2.html)

### 3.2 七个核心 API

| API | 通俗解释 | 返回值或注意点 |
|---|---|---|
| `socket()` | 创建通信端点 | 成功得到 fd；失败 `-1` |
| `bind()` | 给 socket 绑定本地地址和端口 | 端口需要按网络字节序表示，例如 `htons(port)` |
| `listen()` | 把 socket 变成监听 socket | backlog 影响待接受连接队列，不是“服务器终身最多服务人数” |
| `accept()` | 接走一个新连接 | 返回新的 clientfd；不替换 listenfd |
| `recv()` | 从已连接 socket 取字节 | 正数是本次取到的字节数 |
| `send()` | 把字节交给本机 socket 发送路径 | 正数是本次接受的字节数，不保证对端应用已读取 |
| `close()` | 释放本进程的 fd 引用 | 由明确的资源所有者负责，不能多个类随意重复关闭 |

客户端还需要 `connect()` 发起连接。IPv4 常见地址结构是 `sockaddr_in`；其中 IP 和端口需要按 API 要求填写，不能把任意字符串直接当地址结构使用。

### 3.3 为什么 accept 和 recv 不能混在一起

```text
listenfd 就绪 -> 新连接可接受 -> Acceptor::AcceptHandler
clientfd 就绪 -> 连接可读取   -> TcpConnection::ReadHandler
```

同一个 `EPOLLIN` 标志，在不同类型的 fd 上代表不同的处理动作。就像同样是“有人敲门”，办公室大门和个人房门对应的接待人不同。

### 3.4 为什么一个循环服务不了所有阻塞连接

假设代码先接受 A，然后一直阻塞在 `recv(A)`。如果 A 一直不发数据，当前线程就不能回去接受 B，也不能读取已经存在的其他连接。

早期的解决方向是：把连接处理交给其他线程；后来的方向是：用 epoll 统一等待，只有就绪的 fd 才安排处理。

阶段验收：能区分 listenfd 与 clientfd；能完成连接、收发、正常断开；知道“一个客户端不说话”为什么可能阻塞旧版服务器。

---

<a id="s4"></a>
## 4. 阶段三：线程、阻塞队列与线程池探索

这一章回顾早期练习，不把线程池算成最终 V1 的网络调度模型。

### 4.1 一个连接一个线程

```text
主线程：accept A -> 启动工作线程 A -> recv/send A
        accept B -> 启动工作线程 B -> recv/send B
        accept C -> 启动工作线程 C -> recv/send C
```

A 等待数据时，B 的线程仍可运行。代价是线程创建、线程栈和调度开销；连接很多时，大量线程可能大部分时间都在等待 I/O。

### 4.2 线程入口参数到底传了什么

`std::thread` 会保存传入参数供新线程调用使用；普通参数通常按值传递。要共享某个对象，需要明确引用语义及生命周期。

```cpp
// 教学片段：按值复制这一轮的 fd 数字。
int accepted_fd = /* accept 成功返回的 fd */;
std::thread worker([accepted_fd] {
    HandleClient(accepted_fd);
});
// 必须安排 join 或明确的 detach 生命周期；这里省略管理代码。
```

复制 fd 数字不等于复制 socket 资源。两个线程如果都拿着同一个数字，仍然操作同一个进程中的 fd，必须约定谁最终 `close`。

危险形式是把 accept 循环内不断变化的局部变量地址交出去：等线程真正运行时，变量可能已改变或销毁。`pthread_create` 的 `void*` 参数同样不能绕过这个问题。

`join()` 等待线程结束；`detach()` 让线程独立运行，但不会自动延长它捕获的对象寿命。线程对象销毁时仍处于 joinable 状态会触发终止，因此退出路径也属于设计内容。

### 4.3 为什么引入线程池

```text
主线程 accept
      |
      v
任务队列： [连接A] [连接B] [连接C]
      |
      +----> 工作线程1：取任务 -> 执行
      +----> 工作线程2：取任务 -> 执行
      +----> 工作线程3：取任务 -> 执行
```

线程池把“每次创建线程”变成“复用已有线程”。但如果一个任务就是长期阻塞的整条 TCP 连接，所有工作线程仍可能被空闲连接占满。线程池降低创建成本，不会自动消除阻塞 I/O。

### 4.4 阻塞队列的等待与退出

队列为空时，工作线程不应在空循环中反复检查，而应使用条件变量等待。条件变量通知不是任务本身，醒来后必须重新判断共享状态。

```cpp
// 教学伪代码：停止后排空已入队任务，再退出。
for (;;) {
    std::function<void()> task;
    {
        std::unique_lock<std::mutex> lock(mu);
        cv.wait(lock, [&] { return stopping || !tasks.empty(); });
        if (stopping && tasks.empty()) return;
        task = std::move(tasks.front());
        tasks.pop();
    } // 解锁以后才执行业务，避免一个长任务锁住整个队列。
    task();
}
```

停止方在同一互斥锁保护下设置 `stopping`，然后 `notify_all()`，最后 join 所有工作线程。提交任务一侧也要在同步保护下拒绝停止后的新任务。

需要提前定义：停止时“排空任务”还是“丢弃未执行任务”。如果任务正在阻塞 `recv()`，唤醒队列条件变量并不能打断它，需要另行设计连接停止机制。不能一边要求无限等待网络，一边期待线程池立即 join。

阶段验收：能解释参数副本、引用和资源所有权；空队列不空转；空闲 worker 能被停止通知唤醒；不把线程池等同于 Reactor。

---

<a id="s5"></a>
## 5. 阶段四、五：epoll、非阻塞与 Reactor 思维

### 5.1 epoll 解决“等谁”，不替你读写

```text
旧模型：线程A等A，线程B等B，线程C等C

epoll：统一等待 {listenfd, A, B, C, wakefd}
                |
                v
       返回本轮就绪集合 {A, C}
                |
                v
       程序调用 A、C 的处理函数
```

epoll 是 Linux 的 I/O 多路复用接口：管理关注的 fd，并返回就绪事件。Reactor 是围绕这种通知组织代码的模式：等待、分发、调用处理器。两者不是同一个概念。[Linux epoll(7)](https://man7.org/linux/man-pages/man7/epoll.7.html)

### 5.2 epoll 的基本操作

| 操作 | 作用 |
|---|---|
| `epoll_create` / `epoll_create1` | 创建 epoll 实例；历史项目使用过 `epoll_create` |
| `EPOLL_CTL_ADD` | 加入某个 fd 及其监听需求 |
| `EPOLL_CTL_MOD` | 修改已注册 fd 的需求和附带数据 |
| `EPOLL_CTL_DEL` | 移除监听记录 |
| `epoll_wait` | 取得一批就绪事件 |

`epoll_wait` 的 `maxevents` 是数组最多能容纳的**元素数**，不是字节数；返回 `n` 后只遍历 `[0, n)`。超时 `-1` 表示无限等待事件或信号中断；有限超时返回 `0` 并不是错误。[Linux epoll_wait(2)](https://man7.org/linux/man-pages/man2/epoll_wait.2.html)

### 5.3 阻塞与非阻塞

阻塞 socket 当前没有数据时，`recv` 可以等待；非阻塞 socket 则返回“暂时不能读”，把执行权交回调用方。

非阻塞不是反复高速重试。正确方式是暂时不能进行就返回事件循环，等下一次就绪通知。

```cpp
// 教学片段：保留已有标志，再增加 O_NONBLOCK。
bool SetNonBlocking(int fd) {
    int flags = fcntl(fd, F_GETFL, 0);
    if (flags == -1) return false;
    return fcntl(fd, F_SETFL, flags | O_NONBLOCK) != -1;
}
```

监听 fd 和每个接受到的连接 fd 都需要按设计设置。Linux 上普通 `accept()` 返回的新 fd 不自动继承监听 fd 的 `O_NONBLOCK`；也可选择 `accept4` 显式指定，但不能据此宣称原项目已改用它。[Linux accept(2)](https://man7.org/linux/man-pages/man2/accept.2.html)

### 5.4 LT 与 ET

LT 是水平触发：条件仍满足，就有机会继续报告。ET 是边缘触发：重点报告状态变化，通常要求非阻塞并把当前可读、可接受数据处理到 `EAGAIN`，否则可能留下数据却等不到预期新通知。

历史路线从 LT 开始；没有最终源码证明切换为 `EPOLLET`，本文按 **LT 基础上的非阻塞实现**说明 V1，不宣称已采用 ET。循环读到 `EAGAIN` 也不意味着一定用了 ET。

### 5.5 为什么用了 epoll 仍要非阻塞

就绪只是“现在尝试某操作可能取得进展”，不是“整个业务一定完成”。例如读完已有数据后再次 recv，就可能没有更多数据；发送大响应也可能无法一次写完。

如果回调阻塞或执行长时间计算，单 EventLoop 仍会被拖住。V1 的多连接能力有前提：网络处理不阻塞，业务足够短。无限循环读取持续到达的数据也可能影响公平性；每轮处理预算属于后续改进。

---

<a id="s6"></a>
## 6. 阶段六：V1 最终架构与七个核心类

### 6.1 所有权图：谁负责谁活多久

```text
TcpServer
  |
  +-- EventLoop
  |     +-- Epoller --------> epoll 内核实例
  |     +-- wakefd
  |     +-- wakeChannel
  |
  +-- Acceptor
  |     +-- listenfd
  |     +-- acceptChannel
  |
  +-- connections：连接管理表
  |     +-- TcpConnection A
  |     |     +-- clientfd A
  |     |     +-- Channel A
  |     |     +-- InputBuffer / OutputBuffer
  |     +-- TcpConnection B ...
  |
  +-- pending removal：待清理连接集合

Epoller 和活跃事件数组 ----观察----> Channel*
它们不因为保存指针就拥有 Channel，也不负责 delete Channel。
```

这张图表示逻辑资源关系，不规定最终头文件成员声明顺序。销毁连接、Acceptor 时，所需的 EventLoop / Epoller 必须仍然有效。

### 6.2 七个类的职责

| 类 | 核心问题 | 做什么 | 不应混入的责任 |
|---|---|---|---|
| `Acceptor` | 有新连接怎么办？ | 管理监听 socket；可读时 accept；向 TcpServer 交付新 fd | 不替所有客户端 recv，不把监听 fd 当普通连接 |
| `EventLoop` | 什么时候处理谁？ | 循环 Poll、分发 Channel、轮末清理；接受唤醒退出 | 不写具体回复业务，不拥有所有 Channel |
| `Epoller` | 如何与 epoll 交互？ | ADD/MOD/DEL、等待事件、取出就绪 Channel | 不业务处理、不随意销毁连接 |
| `Channel` | 一个 fd 关注哪些事件、如何回调？ | fd、events、revents、注册状态、读写关闭错误回调 | 通常不拥有 fd，不直接保存应用请求 |
| `TcpConnection` | 一条连接怎样读、写、关闭？ | 管理 clientfd、Channel、Buffer、连接状态与读写处理 | 不在自己的回调栈中直接删除自己 |
| `Buffer` | 字节存在哪里、消费多少？ | 追加、查看、统计、消费数据 | 不推断 TCP 消息边界、不控制 epoll |
| `TcpServer` | 如何组织整个服务器？ | 组装模块；新建、登记连接；接收关闭通知；集中清理 | 不在多个层次重复 close/delete |

### 6.3 一条连接从出生到离开

```text
客户端 connect
      |
listenfd EPOLLIN
      |
acceptChannel -> Acceptor::AcceptHandler
      |
accept -> new clientfd -> 设置非阻塞
      |
TcpServer：创建 TcpConnection、绑定回调、登记连接
      |
连接 Channel 注册 EPOLLIN
      |
clientfd EPOLLIN -> ReadHandler -> InputBuffer
      |
Business -> OutputBuffer -> 尝试发送 / 关注 EPOLLOUT
      |
EPOLLOUT -> WriteHandler -> 消费已发送部分
      |
输出为空 -> DisableWrite
      |
EOF / 致命错误 -> CLOSING -> 移除监听 -> 进入待清理集合
      |
本轮所有事件回调返回 -> CleanUp -> 销毁连接、关闭 fd
```

### 6.4 三类 Channel，用一套分发机制

| Channel 对应资源 | 常规关注 | 回调动作 |
|---|---|---|
| listenfd | `EPOLLIN` | accept 新连接 |
| clientfd | `EPOLLIN`，有待发送数据时加 `EPOLLOUT` | recv / send |
| wakefd | `EPOLLIN` | read eventfd，消费通知 |

同一套框架可以处理网络和内部通知，这正是事件抽象的价值。

---

<a id="s7"></a>
## 7. 回调、初始化与一轮事件循环

### 7.1 绑定回调，不等于执行回调

回调就是“先保存一段以后要执行的动作”。`std::function` 可以保存可调用对象，lambda 用来描述这段动作。

```cpp
// 绑定阶段：存下动作，此刻不调用 ReadHandler。
channel.SetReadCallback([this] { this->ReadHandler(); });

// 事件到来以后，Channel 中才真正调用：
if (readcallback_) readcallback_();
```

如果你在关闭时只是创建 lambda 或再次调用 `SetCloseCallback`，实际关闭动作可能从未发生。反过来，初始化时写 `ReadHandler()` 又可能立即执行 I/O，而不是保存动作。

`[this]` 只保存对象指针，不延长对象生命。回调仍可能触发时，对象必须活着。

### 7.2 两条方向相反的链

```text
事件到达：
epoll -> Epoller -> EventLoop -> Channel -> TcpConnection

需求变化：
TcpConnection -> Channel::EnableWrite / DisableWrite
              -> Update 回调 -> EventLoop -> Epoller -> epoll_ctl
```

Epoller 不需要每一轮猜测所有对象是否变化；Channel 在需求变化时主动通知上层更新。

### 7.3 初始化先后顺序

必须先让 fd 有效、字段初始化、所需回调存在，再进行会依赖这些条件的操作。

```text
创建有效资源
   -> Channel 记录 fd
   -> 绑定读写关闭回调
   -> 建立更新回调
   -> 配置监听事件
   -> 成功注册到 epoll
```

注意项目曾采用“先配置 events，再由 AddChannel 绑定 updatecallback 并注册”的方式。这也可以成立，前提是配置函数在尚未注册时不会调用空回调。不能机械规定所有版本都必须“Add 后 EnableRead”，或所有版本都必须相反；要看 API 是否自动发起更新。

最需要维持的约束是：**调用 Update 时回调必须有效；ADD 时 fd、events、data.ptr 必须有效。** 空 `std::function` 应判空或由初始化约束保证，不能依靠侥幸。

### 7.4 一轮循环的骨架

```cpp
// 教学伪代码：轮末清理是生命周期约束的一部分。
while (running.load()) {
    auto active = poller.Poll();
    for (Channel* channel : active) {
        channel->HandlerEvent();
    }
    if (cleanup) cleanup();
}
```

本轮关闭的对象不能在 for 循环中途释放。即使调用 Quit，也要按既定策略完成必要的轮末清理，不能遗留待删除对象。

### 7.5 events 与 revents

`events` 是希望监听什么；`revents` 是本次内核报告发生了什么。它们不必相等。

```cpp
if (revents & EPOLLIN)  { /* 读事件 */ }
if (revents & EPOLLOUT) { /* 写事件 */ }
```

事件是位集合，可以同时出现多个标志；不能拿 `data.fd & EPOLLIN` 判断，也不能只用 `revents == EPOLLIN` 处理所有情况。

不过两个独立 if 之间要考虑状态变化：读回调可能已经逻辑关闭了连接，后面的写回调必须检查连接状态并停止处理。

---

<a id="s8"></a>
## 8. 阶段七 7.1：Channel 生命周期

### 8.1 三个状态描述注册关系

| 状态 | 含义 |
|---|---|
| `NEW` | 对象存在，但尚未成功加入 epoll |
| `ADDED` | 已成功加入 epoll |
| `DELETED` | 已成功从 epoll 移除；C++ 对象仍可存在 |

**DELETED 不表示 `delete` 已经执行。** Channel 的注册状态与对象内存是否有效是两回事。

```text
NEW -------- ADD 成功 ------> ADDED
                               |
                          MOD 成功
                               |
                               v
                             ADDED
                               |
                          DEL 成功
                               |
                               v
                            DELETED
                               |
                     允许重新注册时 ADD 成功
                               |
                               +------------> ADDED
```

连接处于 CLOSING 时，不应被旧回调意外重新注册。Channel 层支持重新 ADD，并不意味着连接层允许任何状态都调用它。

### 8.2 内核成功后才能提交状态

错误顺序：先设置 ADDED、写 map，再调用 `epoll_ctl`。如果系统调用失败，会产生“用户态说已注册，内核实际未注册”的不一致。

正确原则：

```text
检查当前状态 -> 执行内核操作 -> 成功后更新 map 和 Channel 状态
                              -> 失败则保留原状态、向上报告
```

### 8.3 RemoveChannel 的明确契约

历史设计用 bool 表示“这次是否真的成功移除”。在这个契约下：

```cpp
// 教学片段，假设所有操作均在 Loop 线程执行。
bool RemoveChannel(Channel* ch) {
    if (ch->State() != ADDED) return false;
    if (epoll_ctl(epfd, EPOLL_CTL_DEL, ch->Fd(), nullptr) == -1)
        return false;
    channels.erase(ch->Fd());
    ch->SetState(DELETED);
    return true;
}
```

不能移除 NEW 时反而 ADD，不能失败仍 true，也不能非 void 函数走到末尾不 return。

如果未来想让重复移除返回“已经不在 epoll，也算成功”，可以重新设计幂等契约，但调用方和测试必须一起修改。不要把两种 bool 含义混用。

DEL 失败后不能简单进入销毁队列。应记录 fd、状态、errno，判断是否发生重复移除、资源提前关闭等问题；必要时进入明确的失败处理。保留对象避免悬空指针只是最低限度，不是完整恢复机制。

---

<a id="s9"></a>
## 9. 阶段七 7.2：TcpConnection 与延迟销毁

### 9.1 为什么还有第二个状态机

Channel 状态回答“是否注册在 epoll”；连接状态回答“是否还允许业务读写”。

```text
accept 成功 -> CONNECTED -> 请求关闭 -> CLOSING
                                         |
                                   撤销监听并排队
                                         |
                                   轮末真正销毁
```

如果代码保留 `CLOSED` 枚举，它只能表示对象销毁前的逻辑终态。对象销毁后不能再给它 `SetState(CLOSED)`；“内存已经释放”不是可继续写入的对象状态。

### 9.2 use-after-free 是怎样发生的

use-after-free，简称 UAF，表示释放对象后仍访问它。

```text
EventLoop::Distribute()
  -> Channel::HandlerEvent()
      -> readcallback()
          -> TcpConnection::ReadHandler()
              -> CloseHandler()
                  -> TcpServer::RemoveConnection()
                      -> delete connection   [错误：立即释放]
      -> HandlerEvent 继续检查 revents / writecallback
         但 Channel 是 connection 的成员，已经不存在
```

即使 CloseHandler 后面写了 return，也只是返回上一层；外层 `HandlerEvent` 仍可能访问成员。函数返回时，正在执行的 `std::function` 本身也可能属于已经销毁的对象。

### 9.3 V1 的解决思路：逻辑关闭与物理销毁分离

```text
发现 EOF / 致命错误
        |
        v
CloseHandler：CONNECTED -> CLOSING
        |
        v
通知 TcpServer：RemoveChannel
        |
        v
撤销监听成功 -> 待删除集合（去重）
        |
        v
返回当前回调，其他读写入口见 CLOSING 直接退出
        |
        v
本轮全部 Channel 分发结束
        |
        v
CleanUp：移除连接管理记录、销毁对象、资源所有者关闭 fd
```

“延迟”指推迟到安全边界，不是 sleep 一段时间。睡眠无法证明没有旧指针仍在使用。

### 9.4 为什么 DEL 成功仍不能立刻 delete

因为 `epoll_wait` 已经把这一批事件交给用户态了；从内核监听集合移除，不会抹掉 `active` 数组里已有的指针。

如果 A 的回调关闭 B，而 B 也在本轮活跃数组里，那么 B 必须至少活到这轮处理结束；轮到 B 时通过状态检查跳过无效处理。这也是轮末统一清理比“关闭后马上删”安全的原因。

### 9.5 统一关闭、重复保护与所有权

```cpp
// 教学伪代码。
void CloseHandler() {
    if (state != CONNECTED) return;
    state = CLOSING;
    if (closeCallback) closeCallback(fd);
}

void WriteHandler() {
    if (state != CONNECTED) return;
    // ... 非阻塞发送 ...
}
```

必须让 EOF、致命 read/write 错误、相应 HUP/ERR 策略汇入同一关闭入口。`Clear()` 只清 Buffer，不会移除 epoll、清连接表或关闭 fd。

资源责任可约定为：TcpServer 管对象生命周期，TcpConnection 析构负责 clientfd，Channel 只负责事件信息；Acceptor 和 EventLoop 分别管理 listenfd、wakefd。重要的是保持唯一关闭者，不能各层都“顺手 close 一次”。

历史使用待删除 fd 集合时，要防止重复入队，并在 CleanUp 后清空；为了避免 fd 复用造成误删，最好保持旧连接到轮末才关闭其 fd。若提前 close 并允许新的 accept 复用同号 fd，单纯按 fd 删除可能删错对象；更强的设计需要连接身份或代次标识，不能假定数字永远不变。

### 9.6 正常 EOF 与半关闭的边界

在非零接收长度的 TCP recv 中，`ret == 0` 表示对端发送方向已结束；已读到的字节仍然有效。对端也可能只是 `shutdown(SHUT_WR)`，仍在等待接收回复。[Linux recv(2)](https://man7.org/linux/man-pages/man2/recv.2.html)

因此“先收到 hello，再收到 EOF”不能推导出 hello 应被清空。是否继续处理最后的数据、是否排空 OutputBuffer 后关闭，要由应用策略决定。

**V1 的 CLOSING + 延迟销毁主要保证对象安全，不自动保证半关闭回复或全部数据送达。** 如果想实现“对端停止发送后仍完整回复”，还要设计读侧结束、写侧排空等额外状态，并补测试；本文不把它记成已完成。

---

<a id="s10"></a>
## 10. 阶段七 7.3：Buffer 与真实 NUL 字节

### 10.1 Buffer 的输入输出方向

```text
内核接收缓冲区
      |
    recv
      |
      v
InputBuffer -- 查看/处理 --> Business
      |                         |
消费已处理部分                  v
                          OutputBuffer
                                |
                              send
                                |
                      只消费成功发送的部分
```

InputBuffer 保存收到但尚未处理的数据；OutputBuffer 保存计划发送但尚未交给内核的数据。两者不能因为名字都叫 buffer 就混用。

### 10.2 核心思维：地址 + 长度

```cpp
char data[4096];
ssize_t ret = recv(fd, data, sizeof(data), 0);
if (ret > 0) {
    input.Append(data, static_cast<size_t>(ret));
}
```

这里有效范围是 `[data, data + ret)`。`data` 表示起始地址，`ret` 表示字节数。`data[ret] - data[0]` 是两个元素数值的减法，既不是地址范围，也不是字符串切片。

按长度保存不需要额外写 `data[ret] = '\0'`。如果 ret 恰好等于数组容量，写这一句还会越界。

### 10.3 一个便于理解的 Buffer

```cpp
// 教学片段：std::string 能保存 NUL，关键是始终显式传长度。
class Buffer {
public:
    void Append(const char* data, size_t n) {
        if (n != 0) bytes_.append(data, n);
    }
    const char* Data() const { return bytes_.data(); }
    size_t Size() const { return bytes_.size(); }
    bool Empty() const { return bytes_.empty(); }
    void Retrieve(size_t n) {
        assert(n <= bytes_.size());
        bytes_.erase(0, n);
    }
    void RetrieveAll() { bytes_.clear(); }
private:
    std::string bytes_;
};
```

这段采用“消费数量必须合法”的契约，需要包含对应标准头文件。实际项目也可明确定义超过长度就清空，但不要悄悄混用。

这是理解接口的简单实现，不声称是最终源码。频繁从字符串头部 erase 会移动剩余字节，不适合作为“已经达到成熟高性能 Buffer”的证明。读写索引等属于后续优化。

`Data()` 返回的地址不是永久有效凭证。Append、Retrieve 或其他修改后，旧指针可能失效；不要跨修改、跨回调保存它。

### 10.4 查看与消费不同

```text
Buffer = ABCDEFG
Data()       -> 能看到 ABCDEFG，内容没被删除
Retrieve(3)  -> Buffer 变成 DEFG
RetrieveAll()-> Buffer 为空
```

同样，`std::string::erase(pos, count)` 的第二个参数是数量，不是结束下标。把接口设计成 Retrieve(n)，更贴合“消费前 n 字节”。

### 10.5 为什么 string 仍可能截断 NUL

```cpp
std::string wrong("hello\0world");      // 长度 5
std::string right("hello\0world", 11);  // 长度 11

const char sample[] = "hello\0world";
std::string also_right(sample, sizeof(sample) - 1); // 长度 11
```

第一种构造使用 C 字符串规则，在首个 NUL 处停止；不是 `std::string` 存不下 NUL，而是构造时没把后半段放进去。

```text
"hello\0world" 作为数组：
h e l l o [00] w o r l d [00]
|-----------11 个内容字节----------| 末尾自动终止字节

"hello\0world\0"：
5 + 1 + 5 + 1 = 12 个显式内容字节
此外字面量还有一个自动追加的末尾 NUL
```

历史测试中讨论过完整 **12 字节**的 `hello\0world\0`，不能和 11 字节的 `hello\0world` 混为一谈。

### 10.6 send 的地址与长度应来自同一份数据

```cpp
send(fd, right.data(), right.size(), 0);
// c_str() 搭配 size() 也可以按长度发送；问题不在函数名字。
```

错误的是使用 `strlen(right.data())` 重新推断长度，或先 `std::string(data)` 再 substr：数据可能在进入 Buffer 之前已经被截断。

### 10.7 真实 NUL 如何观察

键盘输入 `hello\0world` 通常得到的是可见字符反斜杠与数字零，不是 NUL。应在程序中构造并打印长度、十六进制字节。

```text
hello + NUL + world 的十六进制：
68 65 6c 6c 6f 00 77 6f 72 6c 64
```

直接打印文本看不到 NUL，不足以证明它经过了网络。最可靠的是发送端和接收端比较字节长度及内容。

### 10.8 Business 和 Clear 的正确关系

历史业务曾给回复加上 `Server received --- ` 前缀，然后复制 InputBuffer 内容到 OutputBuffer，再清理已消费输入。这属于演示业务。

应该按这个次序思考：

```text
确认哪些字节可处理 -> 生成输出 -> 消费对应输入
```

不要在尚未处理时 Clear，不要发送失败就无条件清 OutputBuffer，也不要清空输入后再拿输入当“发送内容日志”。

读到 EAGAIN 只说明这一刻没有更多数据，不代表一条协议消息结束。当前“处理本轮全部字节”的演示可以验证 I/O，但不能宣称通用请求解析已经完成。

---

<a id="s11"></a>
## 11. 阶段七 7.4：非阻塞读写与 EPOLLOUT

### 11.1 先看 ret，再看 errno

对于这些 I/O 调用，成功返回的正数表示本次完成字节数；失败通常返回 `-1` 并设置 errno。**仅在失败路径解释 errno**，成功后的 errno 可能是旧值。

| 结果 | 含义 | 典型处理 |
|---|---|---|
| `ret > 0` | 完成了 ret 字节 | 按 ret 追加输入或消费输出 |
| TCP `recv == 0` | 对端发送方向结束 | 执行 EOF 策略、进入统一关闭或排空流程 |
| `EAGAIN / EWOULDBLOCK` | 当前无法立即完成非阻塞操作 | 返回事件循环，等待相应就绪事件 |
| `EINTR` | 系统调用被信号中断 | 重试，不能伪装成“收到数据”或“发送完成” |
| `EPIPE` | 写入已不能正常发送的连接等情形 | 先确保 SIGPIPE 不终止进程，再处理此连接错误 |
| `ECONNRESET` 等 | 连接异常等错误 | 分类记录，按致命错误策略关闭当前连接 |

`EAGAIN` 和 `EWOULDBLOCK` 在 Linux 上通常相同，但可移植写法可以同时判断。正数返回即使小于请求数量，仍表示这部分成功，不要因为“没全完成”就丢弃它。[Linux send(2)](https://man7.org/linux/man-pages/man2/send.2.html)

### 11.2 读循环的思想

```text
ReadHandler
  |
  +-- recv > 0 --------> Append(data, ret) -> 继续读
  +-- recv < 0 EINTR --> 重试
  +-- recv < 0 AGAIN --> 处理当前可处理输入 -> 返回 Loop
  +-- recv == 0 -------> 执行 EOF 策略 -> 关闭/结束本次处理
  +-- 其他致命错误 ----> 统一 CloseHandler -> 返回
```

如果用枚举封装结果，建议让状态清楚表达事实：DATA 是“读到字节”，RETRY 是“重试”，AGAIN 是“暂时停止”，CLOSE 和 ERROR 是关闭原因。不能只因为某个上层 case 恰好 continue，就把 EINTR 命名为 DATA。

读写的 DATA 也未必有同一个业务语义。历史代码把写 DATA 用作“全部发送完成”，所以写 EINTR 返回 DATA 会错误关闭 EPOLLOUT。

### 11.3 partial write 与 EAGAIN 的区别

```text
准备发送 10000 字节：

send 返回 3000
  -> partial write
  -> 这次成功接受了 3000
  -> 保留后 7000

send 返回 -1，errno == EAGAIN
  -> 本次没有成功发送字节
  -> 现有未发送内容全部保留
```

两者可在上层统一表示“还有数据，后续继续”，但日志不能用一个模糊的 AGAIN 代替底层事实。正数 short write 本身不应读取 errno。

### 11.4 为什么 OutputBuffer 必须跨回调存在

临时数组在函数返回后可能销毁，而下一次可写事件发生在未来。剩余数据必须属于连接对象，而不能只是当前函数栈上的临时内容。

```text
OutputBuffer = ABCDEFGHIJ
send 接受 4 字节
Retrieve(4)
OutputBuffer = EFGHIJ

之后收到 EPOLLOUT
send 从 E 开始，不能从 A 重新发送
```

新业务输出到来时也不能覆盖旧的未发送数据，应按顺序追加到它后面。

### 11.5 写处理骨架

```cpp
// 教学伪代码；假设 fd 非阻塞、SIGPIPE 已处理、各入口在 Loop 线程。
void WriteHandler() {
    if (state != CONNECTED) return;
    while (!output.Empty()) {
        ssize_t n = send(fd, output.Data(), output.Size(), 0);
        if (n > 0) {
            output.Retrieve(static_cast<size_t>(n));
            continue;
        }
        if (n == -1 && errno == EINTR) continue;
        if (n == -1 && (errno == EAGAIN || errno == EWOULDBLOCK)) {
            channel.EnableWrite();
            return;
        }
        // 非空请求却没有进展，或其他错误：不能无限空转。
        CloseHandler();
        return;
    }
    channel.DisableWrite();
}
```

历史实现也采用过“部分发送后返回 AGAIN，等待下次 EPOLLOUT”，在 LT 下可以工作。上面的循环是讲解版本，不表示最终代码已重写成这一形式。

### 11.6 EPOLLOUT 是按需订阅

```text
没有待发送字节：只关心读，不常驻监听写
          |
业务生成输出 -> 尝试发送 / 开启写关注
          |
没发完 -> 保存剩余字节 -> 保持 EPOLLOUT
          |
发完 -> OutputBuffer == 0 -> DisableWrite
```

很多 socket 经常可写。如果无数据也一直监听 EPOLLOUT，在 LT 下可能不停收到通知，导致 CPU 空转。`EnableWrite` 只是更新监听需求，不是真正 send；`DisableWrite` 也不是关闭 socket。

### 11.7 EPIPE 与 SIGPIPE 必须分开处理

历史项目采用启动时忽略 SIGPIPE 的方式：

```cpp
// 历史方案的关键语句，启动服务前设置。
signal(SIGPIPE, SIG_IGN);
```

也可以选择 Linux `send(..., MSG_NOSIGNAL)` 屏蔽本次发送的 SIGPIPE，但这是替代方案，不表示项目已经采用。进程级忽略会影响整个进程，库级设计需要考虑这种影响。

只写 `if (errno == EPIPE)` 不够，因为默认 SIGPIPE 行为可能先结束进程。屏蔽后仍要处理 send 失败、清理当前连接，不能认为忽略信号就让发送成功了。

验收目标是“一个连接异常，其他连接仍可用”，不是一定打印 EPIPE。实际可能先观察到 EOF、ECONNRESET 或其他关闭事件。

---

<a id="s12"></a>
## 12. 阶段七 7.5、7.6：data.ptr 与 eventfd 退出

### 12.1 Epoller：从 fd 查找变为直接取 Channel

```text
原来：epoll_wait -> data.fd -> map.find(fd) -> Channel*
现在：epoll_wait -> data.ptr -------------> Channel*
```

```cpp
// ADD 和 MOD 都要完整填写。
epoll_event event{};
event.events = channel->Events();
event.data.ptr = channel;

// Poll 返回后：
Channel* ch = static_cast<Channel*>(events[i].data.ptr);
ch->SetRevents(events[i].events);
active.push_back(ch);
```

`data` 是 union，不能同时把 fd 和 ptr 当两个独立字段保存；写了 ptr 之后再写 fd 会覆盖同一存储。需要 fd 时可以从 Channel 取。

该字段保存的是用户提供的数据，epoll 不负责管理 C++ 对象寿命。[Linux epoll_ctl(2)](https://man7.org/linux/man-pages/man2/epoll_ctl.2.html)

这个优化省去一次事件到 Channel 的映射查找；没有基准测试，不应声称提升了多少百分比。原 map 可以继续用于登记与一致性检查，不一定马上删除。

`data.ptr` 不是智能指针。即使存入 `shared_ptr.get()`，epoll 也不会增加引用计数。它仍依赖注册期间和当前事件批次期间对象存活。

### 12.2 为什么只改 running 不会叫醒 epoll_wait

```text
线程A：while(running) -> epoll_wait(..., -1) -> 等待

线程B：running = false

线程A：还没从 epoll_wait 返回，暂时无机会再检查 running
```

退出需要两件事：共享一个退出条件，以及制造能让等待返回的通知。

### 12.3 atomic<bool> 与 eventfd 各自解决什么

| 机制 | 解决的问题 | 没有解决的问题 |
|---|---|---|
| `atomic<bool>` | 特定状态在跨线程读写时避免数据竞争 | 不会主动唤醒 epoll_wait |
| `eventfd` | 创建可被 epoll 观察的通知 | 不自动保护 connections、Buffer 等其他共享数据 |

V1 支持外部线程 Quit，不代表所有连接接口都变成线程安全。新增、删除连接或修改 Buffer 等操作仍应遵守 Loop 线程归属。

### 12.4 eventfd 的创建与注册

```text
eventfd(0, EFD_NONBLOCK | EFD_CLOEXEC)
       |
检查 fd >= 0
       |
wakeChannel 绑定真实 fd
       |
绑定 WakeUpHandler 读回调，关注 EPOLLIN
       |
通过统一注册路径加入 EventLoop / Epoller
```

`EFD_NONBLOCK` 表示读写不等待；`EFD_CLOEXEC` 表示 exec 时关闭该 fd。它不是“另一个网络客户端”，而是用于通知的内核计数器。

普通模式下，向 eventfd 写入一个 64 位整数会增加计数；成功读取取出累计值并清零。它不是任意字符串消息队列，本文也没有使用 `EFD_SEMAPHORE`。[Linux eventfd(2)](https://man7.org/linux/man-pages/man2/eventfd.2.html)

### 12.5 WakeUp 是 write，WakeUpHandler 是 read

```text
其他线程                              Loop 线程
Quit()
  |
running = false
  |
WakeUp：write(wakefd, 1)
  |                                   epoll_wait 返回
  +----> 计数变为非零 -> EPOLLIN -----> wakeChannel
                                          |
                                      WakeUpHandler
                                          |
                                      read(wakefd)
                                          |
                                      消费通知
                                          |
                                      轮末清理
                                          |
                                      检查 running -> 退出
```

`EnableRead` 是订阅通知，不是消费通知；`EnableWrite` 是关注可写，不是制造唤醒。这里一直关注的是 wakefd 的 EPOLLIN。

### 12.6 两个函数的关键错误分支

```cpp
// 教学片段：eventfd 普通非阻塞模式，fd 生命周期已得到保证。
void WakeUp() {
    uint64_t one = 1;
    for (;;) {
        ssize_t n = write(wakefd, &one, sizeof(one));
        if (n == static_cast<ssize_t>(sizeof(one))) return;
        if (n == -1 && errno == EINTR) continue;
        if (n == -1 && errno == EAGAIN) return;
        ReportWakeFailure(); // 必须暴露故障，不可伪称一定唤醒成功。
        return;
    }
}

void WakeUpHandler() {
    uint64_t count = 0;
    for (;;) {
        ssize_t n = read(wakefd, &count, sizeof(count));
        if (n == static_cast<ssize_t>(sizeof(count))) return;
        if (n == -1 && errno == EINTR) continue;
        if (n == -1 && errno == EAGAIN) return;
        ReportWakeFailure();
        return;
    }
}
```

写 eventfd 遇到 EAGAIN，表示本次加计数会超过可用上限，此时已有待消费通知，不需要像 socket 那样开启 EPOLLOUT 等发送。读 EAGAIN 表示当前没有可消费计数。

一次成功 read 即可消费普通模式下当前累计计数；通知可以合并，不要求十次 WakeUp 就有十次回调。成功后遗漏 return 会使 WakeUp 不断写入；读端若不消费通知则可能持续可读、造成空转。

### 12.7 epoll_wait 被信号中断

```cpp
// 教学片段：非 EINTR 错误交给明确错误策略。
int PollCount() {
    for (;;) {
        int n = epoll_wait(epfd, events, capacity, -1);
        if (n >= 0) return n;
        if (errno == EINTR) continue;
        throw std::system_error(errno, std::generic_category(), "epoll_wait");
    }
}
```

不能把所有负数都当服务器必须立即退出的致命错误，也不能把 `-1` 当事件数量继续遍历。本项目退出依靠 eventfd，因此 EINTR 后重试仍可收到唤醒通知。

### 12.8 Quit、启动与析构的边界

历史结构是 `Loop()` 开始时把 running 设 true，`Quit()` 设 false 并 WakeUp。它的使用前提是：Quit 发生在 Loop 已启动之后。若先 Quit、后 Loop 又写 true，退出请求会被覆盖。

测试可用启动同步确认循环已进入运行阶段。未来也可设计独立的单次停止标志，但不能把这里的改进建议写成已完成代码。

对象必须活到所有可能调用 Quit/WakeUp 的线程结束。主线程 Quit 后，应先 join Loop 线程，再析构 EventLoop；不能在另一个线程仍 write(wakefd) 时 close 它。

```text
请求退出 -> Loop 返回并完成必要清理 -> join
          -> 安全移除剩余 Channel
          -> 移除 wakeChannel -> close(wakefd)
          -> 销毁相关对象及 Epoller
```

成员 Channel 会随所属对象自动析构。不要显式调用 `wakechannel_.~Channel()`，否则正常销毁时可能再次析构。

“唤醒退出成功”只证明阻塞等待能结束；它不代表已经等待所有应用输出送达。生产级 graceful shutdown 是另一项设计与验收任务。

