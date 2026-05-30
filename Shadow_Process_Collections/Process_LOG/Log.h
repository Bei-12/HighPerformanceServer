#include <iostream>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <errno.h>
#include <fcntl.h>
#include <time.h>
#include <string.h>
#include <string>
#include <stdio.h>
#include <stdlib.h>

using namespace std;

#define SIZE 1024
const char* LOG_PATH = "../../logs/";
const char* FIFO_PATH = "../../fifo/";
const char* APP_LOG = "../../logs/app.log";
const char* ERROR_LOG = "../../logs/error.log";
const char* FIFO_LOG = "../../fifo/log_fifo";

enum LogLevel
{
    DEBUG,
    INFO,
    WARNING,
    ERROR,
    FATAL
};

