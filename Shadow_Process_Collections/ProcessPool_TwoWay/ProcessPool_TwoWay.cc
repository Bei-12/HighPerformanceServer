#include "ProcessPool_TwoWay.h"

int CalculateSquare(int number)
{
    return number * number;
}

int main()
{
    vector<Worker> vW;
    // 创建四个子进程
    for (int i = 0; i < NUM; ++i)
    {
        int pipefd1[2] = {0};
        int pipefd2[2] = {0};
        pipe(pipefd1);
        pipe(pipefd2); // 创建两个管道 -- 父进程需要将两个负责读写的管道的fd存入Worker中
        int read_fd_child = pipefd1[0];
        int read_fd_parent = pipefd2[0]; // 用来从子进程中读取结果
        int write_fd_child = pipefd2[1];
        int write_fd_parent = pipefd1[1];
        pid_t id = fork();
        if (id == 0) // 子进程任务 -- 读取数据，进行计算
        {
            // 在读取之前需要将之前的管道关闭
            close(pipefd1[1]);
            close(pipefd2[0]);
            while (1)
            {
                char child[SIZE] = {0};
                int rd = read(read_fd_child, child, SIZE); // 读取数据
                if (rd <= 0)                               // 写端全部关闭
                {
                    close(read_fd_parent);
                    exit(0);
                }
                int number = atoi(child);
                if (number == -1) // 进行销毁
                {
                    close(read_fd_child);
                    close(write_fd_child);
                    exit(0);
                }
                int res = CalculateSquare(number);
                // 将结果传回父进程 -- write
                child[0] = 0;
                sprintf(child, "%d", res); // 将整数转化为字符串
                write(write_fd_child, child, SIZE);
            }
        }
        else
        {
            // 父进程存储子进程的fd -- 父进程要保存两条管道 -- 一条用来传送数据，一条用来读取结果
            close(pipefd1[0]);
            close(pipefd2[1]);
            vW.emplace_back(id, write_fd_parent, read_fd_parent); // 存入
        }
    }

    int index = 0;
    // 父进程进行循环 -- 分发任务
    while (1)
    {
        int num = 0;
        cout << "Assigning values to num:\n"
             << " -1 ------> quit\n"
             << "!(-1) -----> Calculate" << endl;
        cin >> num;
        if (num == -1)
        {
            for (auto &p : vW)
            {
                write(p._pipe_write_fd, "-1", 3);
                close(p._pipe_read_fd);
                close(p._pipe_write_fd);
            }
            break;
        }
        char parent[SIZE] = {0};
        sprintf(parent, "%d", num);
        write(vW[index]._pipe_write_fd, parent, SIZE);
        parent[0] = {0};
        read(vW[index]._pipe_read_fd, parent, SIZE);
        int number = atoi(parent);
        cout << num << "*" << num << " = " << number << endl;
        index++;
        if (index == vW.size())
            index = 0;
    }

    // 循环销毁
    for (int i = 0; i < NUM; ++i)
    {
        int status;
        waitpid(vW[i]._pid, &status, 0);
    }
    return 0;
}