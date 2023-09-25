#include <pthread.h>
#include <stdio.h>
#include <unistd.h>
pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t cond = PTHREAD_COND_INITIALIZER;

void* thread_function(void* arg)
{
    printf("Thread waiting for signal...\n");
    pthread_mutex_lock(&mutex);
    // 所以是设置条件啊 空才去等 
    // 仓库有 直接消费即可
    // 没有signal 或者signal信号在前发送 会阻塞
    pthread_cond_wait(&cond, &mutex);
    printf("Thread received signal!\n");
    pthread_mutex_unlock(&mutex);

    return NULL;
}

int main()
{
    pthread_t thread;

    // 创建线程
    pthread_create(&thread, NULL, thread_function, NULL);

    // 主线程等待一段时间
    printf("Main thread sleeping...\n");
    sleep(1);

    // 发送信号
    printf("Sending signal...\n");
    pthread_mutex_lock(&mutex);
    pthread_cond_signal(&cond);
    printf("这里肯定在线程收到锁之前打印 肯定\n");
    pthread_mutex_unlock(&mutex);

    // 等待子线程结束
    pthread_join(thread, NULL);

    return 0;
}
/*
Main thread sleeping...
Sending signal...
Thread waiting for signal...
*/
