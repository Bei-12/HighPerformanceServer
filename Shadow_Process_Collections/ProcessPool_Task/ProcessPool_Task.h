#include <iostream>
#include <unistd.h>
#include <cstdlib>
#include <vector>
#include <sys/wait.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <string>

#define NUM 4
#define SIZE 256

const char *delimiter = "|";

using namespace std;

long long SquareNumber(int number)
{
    return number * number;
}

long long CubicNumber(int number)
{
    return number * number * number;
}

long long Judge(int number)
{
    if (number % 2 == 0)
        return 0;
    else
        return 1;
}

long long FactorialNumber(int number)
{
    if (number == 0 || number == 1)
        return 1;
    else
        return number * FactorialNumber(number - 1);
}

class Task
{
    // 用户输入 创建Task Task转成字符串 发送
public:
    Task(int type, int number)
        : _type(type),
          _number(number)
    {
    }
    int _type;   // 任务类型
    int _number; // 要处理的数据
    int _result;
    char _buffer[SIZE];
    // 将Task转成字符串 -- 这个任务要在哪里完成？ -- Task中完成

    char *Splicing(int _type, int _number)
    {
        // 将整数转换为字符串
        char *temp = (char *)malloc(sizeof(int) * 5);
        snprintf(_buffer, SIZE, "%d", _type);
        strcat(_buffer, delimiter);
        snprintf(temp, 20, "%d", _number);
        strcat(_buffer, temp);
        free(temp);
        return _buffer;
    }

    int SplitFirst(char *Str)
    {
        char *TempStr = (char *)malloc(sizeof(int) * 4);
        memcpy(TempStr, Str, strlen(Str) + 1);
        char *sp = strtok(TempStr, delimiter);
        int type = stoi(sp);
        free(TempStr);
        return type;
    }

    int SplitSecond(char *Str) // -- number
    {
        char *TempStr = (char *)malloc(sizeof(int) * 4);
        memcpy(TempStr, Str, strlen(Str) + 1);
        char *sp = strtok(TempStr, delimiter);
        char *sn = strtok(NULL, delimiter);
        int number = stoi(sn);
        free(TempStr);
        return number;
    }

    long long Functioncall(int type, int number)
    {
        if (type == 1)
            return SquareNumber(number);
        else if (type == 2)
            return CubicNumber(number);
        else if (type == 3)
            return Judge(number);
        else if (type == 4)
            return FactorialNumber(number);
    }

    ~Task()
    {
        _type = -1;
        _number = -1;
    }
};

class Worker
{
public:
    Worker(pid_t id, int pipe_write_fd, int pipe_read_fd)
        : _id(id),
          _pipe_write_fd(pipe_write_fd),
          _pipe_read_fd(pipe_read_fd)
    {
    }
    pid_t _id;
    int _pipe_write_fd;
    int _pipe_read_fd;
};
