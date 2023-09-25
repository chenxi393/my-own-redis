#pragma once

#include <stdint.h>
#include <stddef.h>
#include <pthread.h>
#include <vector>
#include <deque>


struct Work {
    void (*f)(void *) = NULL; // 这是函数指针 就是这种形式 挺久没见了
    void *arg = NULL;
};

struct TheadPool {
    std::vector<pthread_t> threads;
    std::deque<Work> queue; // mu用来锁住队列
    pthread_mutex_t mu; // 这里用的互斥锁和条件变量
    pthread_cond_t not_empty;
};

void thread_pool_init(TheadPool *tp, size_t num_threads);
void thread_pool_queue(TheadPool *tp, void (*f)(void *), void *arg);
