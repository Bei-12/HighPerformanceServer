#include <iostream>
#include <unistd.h>
#include <cstdlib>
#include <vector>
#include <sys/wait.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>

#define NUM 4
#define SIZE 256

using namespace std;

class Worker{
    public:
    Worker(pid_t id, int pipe_write_fd, int pipe_read_fd)
    :_id(id),
    _pipe_write_fd(pipe_write_fd),
    _pipe_read_fd(pipe_read_fd)
    {}
    pid_t _id;
    int _pipe_write_fd;
    int _pipe_read_fd;
};