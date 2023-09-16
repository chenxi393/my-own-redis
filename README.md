### Contents
Part 1. Getting Started
1. Introduction 
2. Introduction to Sockets
    * syscalls including socket(), bind(), listen(), accept(), read(), write(), close()
3. Hello Server/Client
4. Protocol Parsing        --Text vs. Binary
5. The Event Loop and Nonblocking IO
6. The Event Loop Implementation
7. Basic Server: get, set, del

Part 2. Essential Topics

8. Data Structure: Hashtables
9. Data Serialization
10. The AVL Tree: Implementation and Testing
11. The AVL Tree and the Sorted Set
12. The Event Loop and Timers
13. The Heap Data Structure and the TTL
14. The Thread Pool and Asynchronous Tasks

### 05. The Event Loop and Nonblocking IO
⭐Three ways to deal with concurrent connections
1. forking (creat new processes)
2. multi-threading
3. ⭐event loops (polling 轮询 nonblocking IO 非阻塞IO) usually runs on a single thread!

The simplified pseudo-code for the event loop of our server
```python
all_fds = [...]
while True:
    # poll tell us which fd can be operated without blocking.
    active_fds = poll(all_fds) 
    for each fd in active_fds:
        do_something_with(fd)

def do_something_with(fd):
    if fd is a listening socket:
        add_new_client(fd)
    elif fd is a client connection:
        while work_not_done(fd):
            do_something_to_client(fd)

def do_something_to_client(fd):
    if should_read_from(fd):
        data = read_until_EAGAIN(fd)
        process_incoming_data(data)
    while should_write_to(fd):
        write_until_EAGAIN(fd)
    if should_close(fd):
        destroy_client(fd)
```
> In blocking mode, read blocks the caller when there are no data in the kernel, write blocks when the write buffer is full, and accept blocks when there are no new connections in the kernel queue. In nonblocking mode, those operations either success without blocking, or fail with the `errno EAGAIN`, which means “not ready”.
> The poll is the sole blocking operation in an event loop, everything else must be `nonblocking`; thus, a single thread can handle multiple concurrent connections. All blocking networking IO APIs, such as read, write, and accept, have a nonblocking mode. APIs that do not have a nonblocking mode, such as gethostbyname, and disk IOs, should be performed in `thread pools`, which will be covered in later chapters. Also, timers must be implemented within the event loop since we can’t sleep waiting inside the event loop.

The syscall for setting an fd to nonblocking mode is `fcntl`
* 操作方法的话 是先fcntl get出flag再与非阻塞的flag |= 运算一下 
* 再fcntl 设置flag

poll vs select vs epoll（具体看下面的文章）
* poll和select基本相同 只是select的最大fd数量很小
* epoll API由三个系统调用组成
* epoll 在实际项目中更可取 因为poll fd传参 数量会很多 epoll可能本身包含fd ???
> The epoll API is stateful, instead of supplying a set of fds as a syscall argument, epoll_ctl was used to manipulate an fd set created by epoll_create, which the epoll_wait is operating on.


### 突发奇想地疑问
我们知道 刚刚使用了很多系统调用
那系统调用的unpack 又是是什么

刚刚用的是语言层面的系统调用
下层是linux系统的系统调用？？
如果是 这两者怎么联系起来？？
都是C代码所以之间调用？？
那要是别的语言怎么办？？？
——————于是就去搜了系统调用的实现原理[感觉这文章写的还行](https://www.cnblogs.com/leekun/articles/2628339.html) 

还有一个疑问 如果使用特定语言构建自己的框架或者中间件啥的 基础架构
是不是只使用（或者大部分使用）标准库的东西
标准库再底层的能使用吗？？？

`strace` 查看可执行程序使用的系统调用


### 06.The Event Loop Implementation
* server端的代码改变很多
* client变化不大 就是一次只发一条 变成了连发三条 pipeline
* TODO 记得完成作业 调研epoll  先完成作业再看下一章！！！

### IO多路复用 select poll epoll
1. 使用场景：设计一个高性能的网络服务器（一个服务器要处理多个客户端的请求）
2. 一开始我们会想到使用多线程，但是多线程存在`上下文切换` 消耗太大`❓为什么不使用多进程和协程 这些是不是还分为用户态和内核态` 多线程CPU切换效率更慢
3. 考虑使用单线程 能够保证单线程处理a请求的消息 b的消息不丢失的原因是`dma技术` 这样单线程处理多个请求成为可行的 ❓这里可以再去了解一下dma
4. ulimit -n 单个进程能够监视的文件描述符的数量存在最大限制 1024
5. [下面三种具体代码看这](https://devarea.com/linux-io-multiplexing-select-vs-poll-vs-epoll/#.XYD0TygzaUl)
6. [参考文章](https://segmentfault.com/a/1190000003063859)
#### SELECT
```python
# 一般情况的伪码描述 好像就是select 轮询的模式
while(1){
    for(fd in fdSet){
        if fd 有数据{ # 这一部分效率不高
            读fd;
            处理事件;
        }
    }
}
# 上面是伪码 下面是具体的C语言实现
 while(1){
	FD_ZERO(&rset);# 每一轮开始初始化为0
  	for (i = 0; i< 5; i++ ) {
  		FD_SET(fds[i],&rset);
  	}

   	puts("round again");
	select(max+1, &rset, NULL, NULL, NULL);
# select就对应上面第9行 下面就是用户态自行判断
	for(i=0;i<5;i++) {
		if (FD_ISSET(fds[i], &rset)){
			memset(buffer,0,MAXBUF);
			read(fds[i], buffer, MAXBUF);
			puts(buffer);
		}
	}	
  }
```
select通常使用一个描述符集`rset` bitmap类型 1024大小 1表示启用
上述判断fd是否有数据select是交给内核判断的（这是系统调用）
select 是一个阻塞函数 都没有数据会阻塞
有数据的话 **1.** 会把传入的rset 置为1 **2.** 返回不再阻塞
然后用户态再去循环判断哪一个rset被置位了 if判断之后进行read系统调用 然后处理事件 
**select的缺点** 很古老的API 跨平台是它的优点
1. rset有上限 最大1024 bitmap 并不是linux系统只支持1024个 而是bitmap 32个4字节数组 32*32=1024
2. Fdset 不可重用 每一次循环要置0 效率低
3. 把rset每次拷贝到内核态 虽然比一个一个拷贝询问 效率高（多次变一次性） 但还是有开销（用户态和内核态的数据拷贝切换） 
4. 且内核置位之后 用户态还需要再遍历一遍 复杂度O(n)
![](../Images/2023-09-16-05-36-05.png)

#### POLL
1. int poll (struct pollfd *fds, unsigned int nfds, int timeout);
```c
struct pollfd {
      int fd;		 //连接的fd 文件描述符
      short events;  //关心的事件是什么 POLLIN--读 POLLOUT 写
      short revents; //对事件的回应 response
};
// POLLIN | POLLOUT 表示读写都关心 10 | 01 = 11
```
2. fd中有数据 pollfd.renvents |= POLLIN 也就是置位 然后poll返回
3. 用户态再重置revent 不像select全都重置  
**poll的优缺点**
1. 解决了select  缺点1 和缺点2 pollfds 没有数量上限 且可以重用
2. 缺点就是select 3 4 的缺点 每次循环都要将所有fd拷贝到内核

#### EPOLL
1. epoll_create(这个弹幕说会创建一个红黑树) epoll_ctl（添加连接fd到内核态） epoll_wait（重排获取所有的就绪fd） 
2. 监听的对象是红黑树 epfd代表了所有加进去的（虽然是int但是弹幕说是红黑树的根节点）
3. epoll 不是在原来的基础上置位，而是重排获取所有就绪的fds(用的就绪列表)  没有拷贝？？？ 有吧只是拷贝少？？ O(k)次拷贝？？
4. 时间复杂度O(k)?? O(1)??
5. 我的理解是 epoll_create初始化一个红黑树 epoll_ctl把fd（n个）连接一个个放入内核中的fds（只需拷贝一次） 循环epoll_wait把内核中就绪了的fd（k个）返回给用户态
6. redis和nginx都是epoll实现
7. 解决了上述4个缺点
8. 当遇到大量的idle- connection，就会发现epoll的效率大大高于select/poll。

#### 可以有个待办 
去测试三种并发的连接数 或者类似于QPS的东西 可以网上找找有没有类似的