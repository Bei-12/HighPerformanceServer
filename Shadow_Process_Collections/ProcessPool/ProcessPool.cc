#include "ProcessPool.h"

// 每一个进程有自己独属的匿名管道
// 创建4个匿名管道

int CalculateSquare(int number)
{
    return number * number; // 计算平方数
}

int main()
{
    // 使用类来存储Worker的信息
    // 使用动态数组来存储每个子进程的相关信息
    vector<Worker> vw; // 初始化大小为4
    int count = NUM;
    // 父进程进行分发任务
    // 通过轮询
    int index = 0; // 轮询需要的下标
    // 创建进程池 -- 方便后续调用，分发任务
    while (count > 0)
    {
        int pipefd[2] = {0};
        pipe(pipefd); // 创建匿名管道
        pid_t id = fork();
        int readfd = pipefd[0];
        int worker_fd = pipefd[1];
        if (id == 0)
        {
            close(pipefd[1]);
            // 将写端关闭
            while (1)
            {
                char child[SIZE] = {0};
                ssize_t rd = read(readfd, child, sizeof(child));
                if (rd == 0) // 表示写端全部关闭
                {
                    close(readfd);
                    exit(0);
                }
                else if (rd < 0)
                {
                    perror("read");
                    exit(1);
                }
                int num = atoi(child);
                if (num == -1)
                {
                    close(readfd);
                    exit(0);
                }
                int res = CalculateSquare(num);
                cout << res << endl;
            }
        }
        else
        {
            close(pipefd[0]); // 父进程关闭读端
                              // 关闭上一个子进程的读端，防止数据进行多管道写入
            vw.emplace_back(id, pipefd[1]);
            // 将子进程存入数组中
        }
        count--;
    }
    while (1)
    {
        // 父进程分发任务
        int num = 0;
        cout << "为num赋值：" << endl;
        cout << "-1 ----- quit" << endl;
        cout << "!(-1) ----- calculate " << endl;
        cin >> num;
        if (num == -1)
        {
            for (int i = 0; i < 4; ++i)
            {
                write(vw[i]._pipe_write_fd, "-1", 3); //- 1 \0 
            }
            // 把父进程也关掉
            for (auto &w : vw)
                close(w._pipe_write_fd);
            break;
        } // 退出销毁子进程
        // 开始将已经存储的数据进行分发
        char parent[SIZE] = {0};
        // 爸爸给数据，儿子读
        sprintf(parent, "%d", num);                    // 将整数转为字符，进行传递
        write(vw[index]._pipe_write_fd, parent, SIZE); // 父子进程读写对应的数据长度相等
        index++;
        if (index == vw.size())
            index = 0; // 如果超过进程池中子进程的各树，就置0
    }
    for (int i = 0; i < 4; ++i)
    {
        int status;
        waitpid(vw[i]._pid, &status, 0);
    }
    return 0;
}