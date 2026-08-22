# 《 基于Reactor模型的高性能TCP服务器实现与理解 》

## 1. 项目介绍
    ### 1.1 项目目标
        1. 掌握Linux IO 多路复用机制
        2. 理解Reactor事件驱动模型
        3. 手写基于epoll的Tcp服务器
        4. 实现Channel、EventLoop、Poller等核心组件
        5. 理解muduo网络核心思想

    ### 1.2 项目功能
        |功能|是否完成|
        |-|-|
        |TCP连接建立|√|
        |多客户端支持|√|
        |非阻塞IO|√|
        |epoll事件监听|√|
        |Reactor模型|√|
        |Channel抽象|√|
        |EventLoop事件循环|√|
        |回调机制|√|

## 2. Reactor模型理解
    ### 2.1 Reactor是什么
        Reactor本质：
            事件发生
                ↓
            事件分发
                ↓
            调用对应处理函数
        Reactor不处理业务，只负责事件通知

        ```mermaid
        graph TD
        A[Kernel] --> B[epoll]
        B --> C[Epoller]
        C --> D[EventLoop]
        D --> E[Channel]
        E --> F[TcpConnection]
        F --> 业务

    ### 2.2 为什么需要Reactor
        传统阻塞模型： 一个连接 → 一个线程
        问题：
        - 线程数量有限
        - 创建销毁成本高
        - 大量IO等待浪费CPU

    ### 2.3 Reactor工作流程
        一个线程 --- 管理多个连接 -- 只处理活跃事件

## 3. 整体架构设计
    ### 3.1 类关系
        ```mermaid
        graph TD
        A[TcpServer] --> B[EventLoop]
        B --> C[Epoller]
        C --> D[Channel]
        D --> E[TcpConnection]

    ### 3.2 模块职责
        |模块|职责|
        |-|-|
        |TcpServer|服务器整体管理|
        |EventLoop|事件循环|
        |Epoller|封装epoll|
        |Channel|fd事件抽象|
        |Acceptor|监听连接|
        |TcpConnection|处理客户端连接|

## 4. Epoller设计
    ### 4.1 epoll封装
        不直接在业务代码中调用epoll
        原因：
        - 降低耦合
        - 隐藏系统调用细节
        - 从fd管理转变为对象管理

    ### 4.2 Add Channel
        流程：
        ```mermaid
        graph TD
        A[Channel] --> B[获取fd]
        B --> C[保存fd和Channel映射]
        C --> D[epoll_ctl ADD]

    ### 4.3 UpdateChannel
        ** TcpConnection **
        ```mermaid
        graph ID
        A[修改事件状态] --> B[Channel.events_]
        B --> C[UpdateCallback]
        C --> D[EventLoop]
        D --> E[Epoller.Mod]
        E --> F[内核epoll]

## 5. Channel设计
    ### 5.1 Channel作用
        Channel是fd和事件之间的桥梁
        不负责：
        - socket通信
        - 业务处理

        负责：
        - 保存监听事件
        - 保存实际事件
        - 调用回调

    ### 5.2 events 和 revents
        events_:希望监听什么
        revents_:实际发生什么
        例如：events_ --- EPOLLIN 表示：我希望监听事件 revents_ --- EPOLLIN 表示：内核通知发生读事件

## 6. EventLoop设计
    ### 6.1 工作流程
        while(running)
        {
            epollwait()
                ↓
            获取活跃Channel
                ↓
            HandlerEvent()
        }

    ### 6.2 EventLoop存在的原因
        如果TcpConnection直接操作epoll：业务层和底层强耦合
        EventLoop作为中间层：负责事件调度

## 7. Acceptor连接管理
    listenfd事件流程
    ```mermaid
    graph ID
    A[客户端connect] --> B[listen队列]
    B --> C[listenfd可读]
    C --> D[EPOLLIN]
    D --> E[Acceptor]
    E --> F[accept()]
    F --> G[生成TcpConnection]

## 8. TcpConnection设计
    ### 8.1 读取流程
        client
            ↓
        recv
            ↓
        input_buffer
            ↓
        业务处理

    ### 8.2 发送流程
        业务
            ↓
        output_buffer
            ↓
        send
            ↓
        client
    ### 8.3 非阻塞读取
        recv
            ↓
        数据读取
            ↓
        继续recv
            ↓
        EAGAIN
            ↓
        说明当前没有数据
            ↓
        返回epoll_wait

## 9. 回调机制设计
    channel_.SetReadCallback(read)
    优势：
    - 解耦
    - 扩展方便
    - 符合Reactor思想

## 10. 开发过程中的问题总结
    ### 初始化与对象关系错误
        |错误|出现位置|原因|正确理解|解决方式|
        |-|-|-|-|-|
        |Channel和EventLoop绑定顺序错误|Channel/EventLoop初始化|Channel提前EnableRead，但是updatecallback还没有绑定|Channel修改后需要通知Event Loop，而通知关系必须提前建立|先EventLoop.AddChannel()绑定callback，再EnableRead|
        |Channel默认构造没有初始化|Channel|fd、events_、revents_未初始化|C++成员变量不会自动初始化|使用初始化列表|
        |Acceptor中的listenfd没有初始化|Acceptor|成员变量未初始化|未初始化变量可能保存随机值|listenfd(-1)|
        |TcpConnection和Channel生命周期理解不足|TcpConnection|一开始没有明确谁管理fd|fd属于连接，不属于Channel|TcpConnection关闭fd，Channel只管理事件|
        |Channel不知道EventLoop如何更新|初期设计|想让Channel直接调用Epoller|违反Reactor分层|Channel通过callback通知EventLoop|

    ### Epoll封装错误
        |错误|出现位置|原因|正确理解|解决方式|
        |-|-|-|-|-|
        |AddChannel判断条件错误|Epoller::AddChannel|之前判断GetEvents()>0|Channel可能监听不同事件，不应该固定判断|直接添加Channel|
        |epoll_ctl ADD失败|初期运行|fd=-1传入epoll|epoll只能监听|检查Channel绑定流程|
        |UpdateChannel不知道什么时候调用|初期设计|想在Epooler主动检查Channel|Poller不知道对象状态变化|Channel主动通知|
        |RemoveChannel删除逻辑不完整|RemoveChannel|直接删除fd|删除应该删除内核状态和对象映射|DEL + erase|
        |epoll_ctl DEL传event|Del函数|误以为三个操作参数一致|DEL不需要event|nullptr|
        
    ### Channel设计错误
        |错误|出现位置|原因|正确理解|解决方式|
        |-|-|-|-|-|
        |把fd和事件混在一起理解|初期|认为epoll管理fd|epoll管理的是fd对应事件|Channel作为fd事件抽象|
        |events和revents混淆|初期|不清楚两个状态区别|events=希望监听，revents=实际发生|分离两个变量|
        |Channel承担业务逻辑|初期|想在Channel处理读写|Channel只是事件分发器|业务交给TcpConnection|
        |accept和read混淆|初期|都认为EPOLLIN就是读数据|listenfd EPOLLIN表示连接到达|Acceptor特殊处理|
        |EnableWrite逻辑不清|初期|不理解为什么动态修改|写事件只在发送缓冲区有数据时监听|有数据开启，无数据关闭|

    ### EventLoop设计错误
        |错误|出现位置|原因|正确理解|解决方式|
        |-|-|-|-|-|
        |EventLoop职责划分不清|初期|想让EventLoop处理业务|EventLoop只负责事件循环|Poll + Dispatch|
        |UpdateChannel位置错误|初期|TcpConnection直接找Epoller|对象之间耦合过高|TcpConnection->Channle->EventLoop->Epoller|
        |Loop退出考虑不足|当前遗留|epoll_wait阻塞无法退出|Reactor需要唤醒机制|阶段七eventfd|

    ### TcpConnection错误
        |错误|出现位置|原因|正确理解|解决方式|
        |-|-|-|-|-|
        |recv只读取一次|Read函数|认为一次recv就是完整消息|TCP没有消息边界|循环读取直到EAGAIN|
        |send一次发送完成假设|Write函数|忽略发送缓冲区限制|send可能部分发送|保存output_buffer剩余数据|
        |没有完整关闭流程|Close|close/delete顺序混乱|先移除epoll，再释放资源|RemoveChannel->close->delete|
        |没有处理EINTR|recv/send|只判断EAGIN|系统调用可能被信号打断|增加EINTR|
        |buffer设计简单|string buffer|只能简单测试|高性能服务器需要Buffer类|阶段七优化|

    ### TcpServer错误
        |错误|出现位置|原因|正确理解|解决方式|
        |-|-|-|-|-|
        |初始化流程顺序错误|Init|先修改Channel状态|callback还未建立|调整初始化顺序|
        |连接管理粗糙|connections_map|手动new/delete|生命周期管理复杂|后续智能指针|
        |Accept逻辑理解错误|初期|认为Accept一次即可|非阻塞模式需要循环accept|accept直到EAGAIN|
        |clientfd没有立即加入epoll|初期|创建Connection后忘记注册|新连接必须注册事件|AddChannel|

    ### 客户端错误
        |错误|出现位置|原因|正确理解|解决方式|
        |-|-|-|-|-|
        |Client没有真正多连接|TcpConnection|名称错误|实际还是单连接|创建多个client对象|
        |sendbuffer无效|Client|定义但没有使用|代码冗余|删除|
        |quit判断不完善|Run|字符串协议简单|应设计协议|阶段七完善|
        |reccv关闭处理不足|Receive|return后没有退出|服务器关闭需要结束连接|break|

    ### C++语法与工程错误
        |错误|原因|解决|
        |-|-|-|
        |头文件重复包含|多个hpp互相include|#pragma once|
        |宏port定义重复|TcpServer/TcpConnction都有Port|constexpr或者配置类|
        |lambda捕获错误|[this]、局部变量生命周期问题|明确捕获对象声明周期|
        |make clean使用错误|输入clean而不是make clean|使用Makefile目标|
        |旧.o文件导致错误|修改头文件未重新编译|make clean|

    ### 理解层面的提升点
        |阶段初期理解|现在正确理解|
        |epoll负责处理对象|epoll只负责通知|
        |fd就是服务器对象|fd需要Channle包装|
        |recv就是读取消息|TCP是字节流|
        |Channel管理连接|Channel管理事件|
        |EventLoop处理业务|EventLoop负责调度|
        |多线程解决并发|Reactor通过IO复用解决并发|
        |修改epoll直接调用|对象状态变化->callback->EventLoop->Poller|

    ### 阶段六最大的5个错误
        |错误|价值|
        |-|-|
        |Channel/EventLoop绑定顺序错误|理解Reactor对象关系|
        |不清楚events/revents区别|理解epoll模型|
        |fd生命周期管理混乱|C++服务器核心|
        |recv/send认为一次完成|理解TCP本质|
        |想让底层主动管理上层|学会分层设计|

## 11. TCP服务器运行流程
        Server启动
            ↓
        socket
            ↓
        bind
            ↓
        listen
            ↓
        listenfd加入epoll
            ↓
        EventLoop运行
            ↓
        客户端连接
            ↓
        listenfd触发
            ↓
        accept
            ↓
        创建TcpConnection
            ↓
        clientfd加入epoll
            ↓
        客户端发送数据
            ↓
        clientfd触发
            ↓
        Channel调用ReadCallback
            ↓
        TcpConnection处理业务

## 12. 阶段总结
    阶段六最大收获：
        以前：认为epoll是一个API
        现在：
            理解 --- epoll只是事件通知机制
            真正的服务器需要：对象管理 + 事件分发 + 生命周期管理 + 回调机制 
        Reactor模型解决的是：如何管理大量连接

## 13. 下一阶段计划
    Stage7_Reactor优化
    - Buffer
    - Connection生命周期
    - eventfd退出
    - Timer
    - Logger
    - ThreadPool结合