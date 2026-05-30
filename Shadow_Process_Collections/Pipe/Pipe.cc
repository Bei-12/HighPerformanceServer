// #include "Uname_Pipe.h"
#include <iostream>
#include <unistd.h>
#include <fcntl.h>
#include <cstdlib>
#include <string>

using namespace std;

#define NUM 10
#define size 1024

// 思路：
// 先组织，再描述
// 父进程创建子进程，父进程通过管道对子进程的写端进行输入
// 父进程关闭读端，子进程关闭写端，父进程的写端重定向(指向管道)，子进程的读端重定向(由管道提供文件数据)
// 重定向需要的 --- 重定向函数 文件描述符
// 在创建子进程之前创建管道 --> 父子才能共享管道的两端
// 注意要关闭当前子进程之前子进程的重定向 --- 通过拷贝会留下来，要关闭

int main()
{
    // 父进程 -- 现在的主函数就是一个进程

    // 创建管道 + 创建子进程
    int count = 0;
    while (count <= NUM)
    {
        int pipefd[2];
        int pipe_id = pipe(pipefd); // 因为要进行进程间通信，所以必须要将匿名管道的读端和写端都打开
        if (pipe_id == -1)
        {
            //perror("Create a pipe fail"); // 打印错误码所对应的信息应该也行
            exit(1);
        }
        pid_t id = fork();
        if (id != 0) // 说明当前运行的是子进程，关闭父进程读端，子进程写端，将父进程的写端(文件标识符)重定向到匿名管道中，子进程的读端(文件标识符)重定向到匿名管道中
        {
            // 重定向
            // 将子进程的读端指向匿名管道，匿名管道对子进程进行数据输入
            // 关闭输入端，向显示器输入的文件标识符 -- 关闭子进程的写端
            close(pipefd[1]);
            // 关闭子进程写端
            char child[size] = {};
            read(pipefd[0], child, sizeof(child));
            cout << "Message From Father:    "<<child<<endl;
            exit(0);
        }
        else // 父进程代码执行
        {
            close(pipefd[0]);   // 关闭父进程的读端
            cout << "Please Input Some Message:    ";
            char ch[size] = {};
            cin>>ch;
            write(pipefd[1], ch, sizeof(ch));
            cout << "Message To Child:" << ch << endl;
        }
        count++;
    }

    // 由父进程对匿名管道进行输入数据，子进程从匿名管道中读取数据 -- 通过文件标识符来实现

    return 0;
}