#include <assert.h>
#include "thread_pool.h"


static void *worker(void *arg) {
    TheadPool *tp = (TheadPool *)arg;
    while (true) { // 4个线程一直循环去等这个任务
        pthread_mutex_lock(&tp->mu);
        // 更底层的实现应该是硬件中断
        // wait for the condition: a non-empty queue
        while (tp->queue.empty()) {
            // 这里会释放互斥锁tp->mu 这一块有点忘了
            pthread_cond_wait(&tp->not_empty, &tp->mu);
            // 其实也会等下面放锁之后 才会拿到两把锁
        }// 这里为什么也要循环（而不用if）操作系统学了（有点忘了）
        // 再解释一下 因为生产者发信号后 可能有其他消费者拿到了
        // TODO ❓ 这里其实还是有疑问 生产者我们用的是signal
        // 按道理 应该是下面的原因 至少唤醒一个 反正有风险
        // while直接避免了风险  TODO 可以再去探究一下
        // pthread_cond_signal 应该是至少唤醒一个等待的线程
        // 而 pthread_cond_broadcast 则会唤醒所有等待的线程。
        // 无论如何 条件变量一定要循环使用！！！

        // got the job
        Work w = tp->queue.front();
        tp->queue.pop_front();
        pthread_mutex_unlock(&tp->mu);

        // do the work
        w.f(w.arg);
    }
    return NULL;
}

void thread_pool_init(TheadPool *tp, size_t num_threads) {
    assert(num_threads > 0);
    // 初始化mutex 和con_t
    int rv = pthread_mutex_init(&tp->mu, NULL);
    assert(rv == 0);
    rv = pthread_cond_init(&tp->not_empty, NULL);
    assert(rv == 0);

    tp->threads.resize(num_threads);
    for (size_t i = 0; i < num_threads; ++i) {
        // 这里就创建了 num_threads个线程 去执行worker函数 参数是tp
        int rv = pthread_create(&tp->threads[i], NULL, &worker, tp);
        assert(rv == 0);
    }
}

void thread_pool_queue(TheadPool *tp, void (*f)(void *), void *arg) {
    Work w;
    w.f = f;
    w.arg = arg;

    pthread_mutex_lock(&tp->mu);
    tp->queue.push_back(w);
    pthread_cond_signal(&tp->not_empty);// 生产者唤醒 等待的
    // siganl放到unlock后也是对的 
    // signal至少唤醒一个线程
    pthread_mutex_unlock(&tp->mu);
}
