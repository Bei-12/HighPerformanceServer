#include "Log.h"

// 思路：通过进程池来实现日志
// 进程池 -- 创建一堆等待被使用的进程
// 日志要求：日志时间 日志的等级 日志内容 文件的名称和行号
// 日志的等级：
// Info -- 常规消息
// Warning -- 报警消息
// Error -- 相当严重，可能需要立即处理
// Fatal -- 致命的
// Debug -- 调试
// main进程 -- fork() 子进程read 父进程write

int main()
{
    // 创建命名管道 -- 判断命名管道是否存在 不存在就创建 存在就不管
    if (mkfifo(FIFO_LOG, 0666) == -1)
    {
        if (errno == EEXIST)
        {
            cout << "fifo already exists" << endl;
        }
        else
        {
            perror("mkfifo");
            exit(1);
        }
    }

    // 打开管道对文件进行追加写入

    pid_t id = fork();
    // 创建子进程
    // 子进程执行的任务 -- 读取FIFO 分类日志 写文件

    if (id == 0)
    {
        int fifo_fd = open(FIFO_LOG, O_RDONLY);
        int app_fd = open(APP_LOG, O_WRONLY | O_CREAT | O_APPEND, 0666);
        int error_fd = open(ERROR_LOG, O_WRONLY | O_CREAT | O_APPEND, 0666);
        char child[SIZE + 1] = {0};
        while (1)
        {
            ssize_t ret = read(fifo_fd, child, SIZE); // 读取管道中的文件
            //cout << "read:" << child <<endl; 
            if (ret > 0)
            {
                child[ret] = '\0';
                char log_buf[SIZE + 1];
                strcpy(log_buf, child);
                log_buf[ret] = '\0';
                char *token;
                token = strtok(child, "|");
                if (token != NULL)
                {
                    if (strcmp(token, "INFO") == 0 || strcmp(token, "DEBUG") == 0 || strcmp(token, "WARINING") == 0)
                    {
                        // 将信息写入app.log中
                        write(app_fd, log_buf, strlen(log_buf));
                    }
                    else if (strcmp(token, "FATAL") == 0 || strcmp(token, "ERROR") == 0)
                    {
                        write(app_fd, log_buf, strlen(log_buf));
                        write(error_fd, log_buf, strlen(log_buf));
                    }
                }
            }
        }
    }
    // 父进程执行的任务 -- 执行业务逻辑，产生日志，发送日志
    else
    {
        int fifo_fd = open(FIFO_LOG, O_WRONLY);
        char ch[SIZE] = {0};
        // 将日志信息写入string中
        string s;
        while (1)
        {
            cout << "Please enter the log level:\n";
            cin >> s;
            // 写的时候进行分类处理 -- 在此处直接使用Log.h中的枚举的字符串 ！！！-- 已经进行判断
            if (s == "INFO")
            {
                char time_buf[64] = {0};
                char buf[SIZE] = {0};
                time_t t = time(NULL);
                struct tm *tm_info = localtime(&t);
                strftime(time_buf, sizeof(time_buf), "%Y-%m-%d %H:%M:%S", tm_info);
                cout << "Please enter the message:\n";
                cin.ignore();
                cin.getline(ch, SIZE);
                snprintf(buf, sizeof(buf), "INFO|%s|%s:%d|%s\n", time_buf, __FILE__, __LINE__, ch);
                //cout << "send:" << buf <<endl;
                write(fifo_fd, buf, strlen(buf));
            }
            else if (s == "DEBUG")
            {
                char time_buf[64] = {0};
                char buf[SIZE] = {0};
                time_t t = time(NULL);
                struct tm *tm_info = localtime(&t);
                strftime(time_buf, sizeof(time_buf), "%Y-%m-%d %H:%M:%S", tm_info);
                cout << "Please enter the message:\n";
                cin.ignore();
                cin.getline(ch, SIZE);
                snprintf(buf, sizeof(buf), "DEBUG|%s|%s:%d|%s\n", time_buf, __FILE__, __LINE__, ch);
                write(fifo_fd, buf, strlen(buf));
            }
            else if (s == "WARINING")
            {
                char time_buf[64] = {0};
                char buf[SIZE] = {0};
                time_t t = time(NULL);
                struct tm *tm_info = localtime(&t);
                strftime(time_buf, sizeof(time_buf), "%Y-%m-%d %H:%M:%S", tm_info);
                cout << "Please enter the message:\n";
                cin.ignore();
                cin.getline(ch, SIZE);
                snprintf(buf, sizeof(buf), "WARNING|%s|%s:%d|%s\n", time_buf, __FILE__, __LINE__, ch);
                write(fifo_fd, buf, strlen(buf));
            }
            else if (s == "FATAL")
            {
                char time_buf[64] = {0};
                char buf[SIZE] = {0};
                time_t t = time(NULL);
                struct tm *tm_info = localtime(&t);
                strftime(time_buf, sizeof(time_buf), "%Y-%m-%d %H:%M:%S", tm_info);
                cout << "Please enter the message:\n";
                cin.ignore();
                cin.getline(ch, SIZE);
                snprintf(buf, sizeof(buf), "FATAL|%s|%s:%d|%s\n", time_buf, __FILE__, __LINE__, ch);
                write(fifo_fd, buf, strlen(buf));
            }
            else if (s == "ERROR")
            {
                char time_buf[64] = {0};
                char buf[SIZE] = {0};
                time_t t = time(NULL);
                struct tm *tm_info = localtime(&t);
                strftime(time_buf, sizeof(time_buf), "%Y-%m-%d %H:%M:%S", tm_info);
                cout << "Please enter the message:\n";
                cin.ignore();
                cin.getline(ch, SIZE);
                snprintf(buf, sizeof(buf), "ERROR|%s|%s:%d|%s\n", time_buf, __FILE__, __LINE__, ch);
                write(fifo_fd, buf, strlen(buf));
            }
            else if (s == "QUIT")
                break;
            else
                break;
        }
    }
    return 0;
}