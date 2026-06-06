#include <iostream>
#include <vector>
#include <cstdlib>
#include <unistd.h>
#include <sys/wait.h>

using namespace std;

#define NUM 4
#define SIZE 64

class Worker
{
public:
    Worker(pid_t pid, int pipe_write_fd, int pipe_read_fd)
        : _pid(pid),
          _pipe_write_fd(pipe_write_fd),
          _pipe_read_fd(pipe_read_fd)
    {
    }
    pid_t _pid;         // 用来销毁子进程
    int _pipe_write_fd; // 用来传送任务
    int _pipe_read_fd;  // 用来接受结果

    ~Worker()
    {
    }
};