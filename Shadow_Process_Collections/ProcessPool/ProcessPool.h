#include <vector>
#include <iostream>
#include <time.h>
#include <unistd.h>
#include <string.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <stdlib.h>

using namespace std;

#define NUM 4
#define SIZE 128

class Worker
{
    // 初始化列表进行初始化
public:
    Worker(pid_t pid, int pipe_write_fd)
        : _pid(pid),
          _pipe_write_fd(pipe_write_fd)
    {
    }
    pid_t _pid;
    int _pipe_write_fd;

    ~Worker()
    {
        // 销毁进程池中的子进程
        _pid = 0;
        _pipe_write_fd = 0;
    }
};
