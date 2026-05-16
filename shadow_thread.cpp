#include <pthread.h>
#include <unistd.h>
#include <stdio.h>
#include <iostream>
#include <string>
#include <cstdlib>
using namespace std;

// static int ret1=1;
// static int ret2=2;
static int ret0=0;

//#inlcude "apue.h"//这个库文件是个啥

// pthread_t ntid;

// void printids(const char *s)
// {
//     pid_t pid;
//     pthread_t tid;
//     pid = getpid();//获得进程id
//     tid = pthread_self();//获得线程id
//     printf("%s pid %lu tid %lu (0x%lx)\n\n", s, (unsigned long)pid, (unsigned long)tid, (unsigned long)tid);
// }

// void* thr_fn(void *arg)
// {
//     printids("new thread:");//这个是个什么函数？-- 打印线程id的函数
//     return ((void *)0);
// }

// void* thr_fn1(void *arg)
// {
//     cout<<"thread 1 returning"<<endl;
//     return (void*)&ret1;
// }

// void* thr_fn2(void *arg)
// {
//     cout<<"thread 2 exiting"<<endl;
//     pthread_exit((void*)&ret2);//??
// }

void err_exit(int err,const char *s)
{
    if(err!=0)
    {
        cout<<s<<endl;
        exit(1);
    }
}

struct foo{
    int a,b,c,d;
};

void printfoo(const char *s, const struct foo *fp)//验证自动变量（分配在栈上）作为pthread_exit的参数时产生的问题
{
    printf("%s",s);
    printf("Structure at 0x%lx\n",(unsigned long)fp);
    printf(" foo.a = %d\n",fp->a);
    printf(" foo.b = %d\n",fp->b);
    printf(" foo.c = %d\n",fp->c);
    printf(" foo.d = %d\n",fp->d);
}

void* thr_fn3(void* arg)//验证自动变量（分配在栈上）作为pthread_exit的参数时产生的问题
{
    struct foo foo={1,2,3,4};
    printfoo("thread 3:\n",&foo);
    pthread_exit((void*)&foo);
}

void* thr_fn4(void* arg)//验证自动变量（分配在栈上）作为pthread_exit的参数时产生的问题
{
    printf("thread 2:ID is %lu\n",(unsigned long)pthread_self());
    pthread_exit((void*)&ret0);
}

int main()
{
    int err;
    pthread_t tid1,tid2;
    struct foo *fp;
    // void* tret1=NULL;
    // void* tret2=NULL;
    err = pthread_create(&tid1, NULL,thr_fn3, NULL);//创建新线程
    //printf("err:%d\n",err);
    if(err != 0) err_exit(err,"can not create thread 3");
    err = pthread_join(tid1,(void**)&fp);
    if(err != 0) err_exit(err,"can't join with thread 3");
    sleep(1);

    cout<<"Parent starting second thread\n";
    err = pthread_create(&tid2, NULL,thr_fn4, NULL);
    if(err != 0) err_exit(err,"can not create thread 2");
    
    sleep(1);
    printfoo("Parent:\n",fp);
    return 0;
}
