#include "ProcessPool_Framework.h"

char delimiter[] = "|";

long long SquareNumber(long number)
{
    return number * number;
}

long long CubeNumber(long number)
{
    return number * number * number;
}

long long Judge(long number)
{
    if (number % 2 == 0)
        return 1; // 表示是偶数
    else
        return 0;
}

long long FactorialNumber(int number)
{
    if (number == 1 || number == 0)
        return 1;
    else
        return number * FactorialNumber(number - 1);
}

long long Task(int type, int number)
{
    switch (type)
    {
    case 1:
        // 计算平方
        return SquareNumber(number);
    case 2:
        // 计算立方
        return CubeNumber(number);
    case 3:
        // 判断奇偶
        return Judge(number);
    case 4:
        // 计算阶乘
        return FactorialNumber(number);
    }
}

int main()
{
    // 计算平方 计算立方 判断奇偶 计算阶乘
    vector<Worker> vW;
    for (int i = 0; i < NUM; ++i)
    {
        int pipefd1[2] = {0}; // 传送数据
        int pipefd2[2] = {0}; // 接收数据
        pipe(pipefd1);
        pipe(pipefd2);
        int readfd = pipefd1[0];
        int writefd = pipefd2[1];
        pid_t id = fork();
        // 创建双向管道
        if (id == 0) // 子进程执行任务
        {
            close(pipefd1[1]); // 0 - 读 1 - 写
            close(pipefd2[0]);
            for (auto &w : vW) // 关掉之前的管道，防止数据多管道写入
            {
                close(w._pipe_read_fd);
                close(w._pipe_write_fd);
            }
            while (1) // 进行任务的接受
            {
                char child[SIZE] = {0};
                int rd = read(readfd, child, SIZE);
                if (rd == 0) // 说明写端关闭
                {
                    close(readfd);
                    exit(0);
                }
                else if (rd < 0)
                {
                    perror("read");
                    exit(0);
                }
                // 将字符串转化为整数 -- 我需要读到两个数 -- type number
                // 我要怎么将数字传过去？分割符是什么？ -- '|' 用这个当分隔符
                int type = 0, number = 0;
                char *Type = (char *)malloc(SIZE);
                // char *Number = (char *)malloc(SIZE);
                Type = strtok(child, delimiter); // 用来存储type
                type = stoi(Type);
                Type = strtok(NULL, delimiter); // 用来存储number
                // 将两个字符串转换为整数
                number = stoi(Type);
                // free(Number);
                //  对number进行判断和运算
                if (type == -1)
                {
                    // 销毁 + 退出
                    close(readfd);
                    exit(0);
                }
                else if (type == 1 || type == 2 || type == 3 || type == 4) // 是否需要将选择和结果一起传回去？？？
                {
                    long long res = Task(type, number);
                    // 将整数转化为字符串
                    cout << res << endl;
                    memset(child, 0, sizeof(child));
                    sprintf(child, "%d", type);
                    strcat(child, delimiter);
                    char *tem = (char *)malloc(SIZE);
                    sprintf(tem, "%lld", res);
                    strcat(child, tem);
                    // 将选项和结果传递回去 !!!!
                    int wt = write(writefd, child, SIZE);
                    if (wt == 0)
                    {
                        perror("write");
                        exit(0);
                    }
                    free(tem);
                }
                else
                {
                    cout << "输入错误的信息" << endl;
                    break;
                }
            }
        }
        else
        { // 父进程生成任务
            close(pipefd1[0]);
            close(pipefd2[1]);
            vW.emplace_back(id, pipefd1[1], pipefd2[0]); // 将id fd存入worker中
        }
    }

    int index = 0;
    while (1) // 父进程进行任务的分配 -- 按下标来
    {
        int type = 0, number = 0;
        char parent[SIZE] = {0};
        cout << "==========  1. 计算平方数 ==========" << endl;
        cout << "==========  2. 计算立方数 ==========" << endl;
        cout << "==========  3. 判断奇偶性 ==========" << endl;
        cout << "==========  4. 计算阶乘   ==========" << endl;
        cout << "========== -1. 退出       ==========" << endl;
        cout << "              输入选择              " << endl;
        cin >> type;
        if (type == 1 || type == 2 || type == 3 || type == 4)
        {
            cout << "请输入要进行处理的数字: ";
            cin >> number;
            // 将数据传递给子进程进行处理
            // 将整数转换为字符串
            // 将数字转换为字符串然后再传递过去 -- type和number
            char parent[SIZE] = {0};
            char *Type = (char *)malloc(SIZE); // 分配空间 用来存储
            char *Number = (char *)malloc(SIZE);
            snprintf(Type, SIZE, "%d", type);
            snprintf(Number, SIZE, "%d", number);
            strcpy(parent, Type);
            strcat(parent, delimiter);
            strcat(parent, Number);
            int wt = write(vW[index]._pipe_write_fd, parent, SIZE); // 传递数据
            if (wt == 0)
            {
                perror("write");
                exit(0);
            }
            // 读取数据
            // 进行分割 -- 输出结果
            memset(parent, 0, sizeof(parent));
            int rd = read(vW[index]._pipe_read_fd, parent, SIZE);
            if (rd == 0)
            {
                for (auto &v : vW)
                {
                    close(v._pipe_read_fd);
                }
                exit(0);
            }
            else if (rd < 0)
            {
                perror("read");
                exit(1);
            }
            char *symbol = strtok(parent, delimiter); // 用来存储type
            int logo = stoi(symbol);
            symbol = strtok(NULL, delimiter); // 用来存储number
            // 将字符串转换为整数
            long long result = stoll(symbol); // stoi的返回值为int
            if (symbol == nullptr)
            {
                continue;
            }
            if (logo == 1)
            {
                cout << number << "的平方数为:" << result << endl;
            }
            else if (logo == 2)
            {
                cout << number << "的立方数为:" << result << endl;
            }
            else if (logo == 3)
            {
                if (result == 1)
                {
                    cout << number << "是偶数" << endl;
                }
                else if (result == 0)
                {
                    cout << number << "是奇数" << endl;
                }
            }
            else if (logo == 4)
            {
                cout << number << "的阶乘为:" << result << endl;
            }
            index++;
            if (index == vW.size())
                index = 0;
        }
        else if (type == -1) // 直接退出
        {
            cout << "quit" << endl;
            for (auto v : vW)
            {
                // 传递消息
                write(v._pipe_write_fd, "-1|-1", 6);
                close(v._pipe_read_fd);
                close(v._pipe_write_fd);
            }
            break;
        }
    }

    for (auto &v : vW)
    {
        int status = 0;
        waitpid(v._id, &status, 0);
    }
    return 0;
}