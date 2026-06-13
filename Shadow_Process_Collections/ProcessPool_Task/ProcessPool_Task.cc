#include "ProcessPool_Task.h"

int main()
{
    // 父进程 -> Task -> 子进程读取
    vector<Worker> vW;
    for (int i = 0; i < NUM; ++i)
    {
        int pipefd1[2] = {0};
        int pipefd2[2] = {0};
        pipe(pipefd1);
        pipe(pipefd2);
        int readfd = pipefd1[0];
        int writefd = pipefd2[1];
        pid_t id = fork();
        if (id == 0)
        {
            close(pipefd1[1]);
            close(pipefd2[0]);
            for (auto &w : vW)
            {
                close(w._pipe_read_fd);
                close(w._pipe_write_fd);
            }
            while (1)
            {
                // 将传递过来的信息进行解析 计算 传递
                // 将结果读出来
                Task ts(0, 0);
                char child[SIZE] = {0};
                int rd = read(readfd, child, SIZE);
                if (rd == 0) // 读端关闭
                {
                    close(readfd);
                    exit(0);
                }
                if(strcmp(child, "-1|-1") == 0)
                {
                    close(readfd);
                    close(writefd);
                    exit(0);
                }
                // 将读到的信息进行分析
                int type = ts.SplitFirst(child);
                int number = ts.SplitSecond(child);
                if (type == 1 || type == 2 || type == 3 || type == 4)
                {
                    long long result = ts.Functioncall(type, number);
                    // 将结果传递过去
                    memset(child, 0, strlen(child) + 1);
                    snprintf(child, SIZE, "%lld", result);
                    int wt = write(writefd, child, SIZE);
                    if (wt == 0)
                    {
                        perror("write");
                        exit(1);
                    }
                }
                // else if (type == -1)
                // {
                //     close(readfd);
                //     close(writefd);
                //     exit(0);
                // }
                else
                {
                    perror("Transmission error");
                    exit(1);
                }
            }
        }
        else
        {
            close(readfd);
            close(writefd);
            vW.emplace_back(id, pipefd1[1], pipefd2[0]);
        }
    }

    int index = 0;
    while (1)
    {
        int type = 0;
        int number = 0;
        cout << " ========== 1. 计算平方数 ========== " << endl;
        cout << " ========== 2. 计算立方数 ========== " << endl;
        cout << " ========== 3. 判断奇偶性 ========== " << endl;
        cout << " ========== 4. 计算阶乘   ========== " << endl;
        cout << " ========== -1. 退出      ========== " << endl;
        cout << "输入选择:";
        cin >> type;
        if (type == 1 || type == 2 || type == 3 || type == 4)
        {
            cout << "输入要处理的数字：(范围:[0,20])";
            cin >> number;
            Task ts(type, number);
            // 得到拼接后的字符串 传递给子进程进行处理
            char *parent = ts.Splicing(type, number);
            int wt = write(vW[index]._pipe_write_fd, parent, SIZE); // 传递
            if (wt < 0)
            {
                for (auto &w : vW)
                {
                    close(w._pipe_write_fd);
                }
                exit(1);
            }
            memset(parent, 0, strlen(parent) + 1);
            int rd = read(vW[index]._pipe_read_fd, parent, SIZE);
            if (rd == 0) // 说明写端关闭
            {
                for (auto &w : vW)
                {
                    close(w._pipe_read_fd);
                }
                exit(1);
            }
            long long result = stoll(parent);
            if (type == 1)
            {
                cout << number << "的平方数为:" << result << endl;
            }
            else if (type == 2)
            {
                cout << number << "的立方数为:" << result << endl;
            }
            else if (type == 3)
            {
                if (result == 0)
                    cout << number << "是偶数" << endl;
                else
                    cout << number << "是奇数" << endl;
            }
            else if (type == 4)
            {
                cout << number << "的阶乘为:" << result << endl;
            }
            index++;
            if (index == vW.size())
                index = 0;
        }
        else if (type == -1)
        {
            cout << "quit" << endl;
            for (auto &w : vW)
            {
                write(w._pipe_write_fd, "-1|-1", 6); // 传递
            }
            break;
        }
        else
        {
            perror("输入错误 --> cin");
            exit(0);
        }
    }

    for (auto &w : vW)
    {
        int status = 0;
        waitpid(w._id, &status, 0);
    }

    return 0;
}