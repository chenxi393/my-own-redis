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

### 05 三种方式处理并发连接
1. fork创建新进程
2. 多线程
3. ⭐事件循环 (polling 轮询 nonblocking IO 非阻塞IO) 通常单线程使用 业务中常用

poll伪代码实现
```python
all_fds = [...]
while True:
    # poll告诉我们哪个fd可以操作且不被阻塞
    # 也就是内核告诉我们哪个fd已经有数据了
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
> 在阻塞模式下，当内核没有数据时，read()会阻塞调用者，但内核写缓冲区已满，write()会阻塞调用者，当没有新的连接在内核队列里，accpt也会阻塞调用者

>非阻塞模式下，上述系统调用只会非阻塞的返回成功，或者返回失败带着`errno EAGAIN`（表示数据没准备好，下次再试） 但是我们认为非阻塞系统调用也是同步的，和异步的系统调用有本质区别，因为内核中有数据也需要等待CPU将内核缓冲区的数据拷贝到用户的缓冲区，没数据也得循环系统调用，这本质上是同步的。而异步是拷贝完给程序发信号，可以再去了解一下`信号机制`，Signal，`回调函数`.

> 在事件循环中，`select` | `poll` | `epoll` 是唯一的阻塞操作，其余操作都是非阻塞的。这就是为什么单线程能处理多个并发连接。所有的网络IO APIs都有非阻塞模式。使用前，我们需要使用fcntl 将fd设置为非阻塞的。

> 那些没有非阻塞模式的API，例如`gethostbyname`，`disk IOs`，应该放在`线程池`里去执行，最后一章会说。计时器也应该在事件循环里去实现，因为我们不能sleep为了等待事件循环（since we can’t sleep waiting inside the event loop.）。

The syscall for setting an fd to nonblocking mode is `fcntl`
* 操作方法的话 是先fcntl get出flag再与非阻塞的flag |= 运算一下 
* 再fcntl 设置flag

poll vs select vs epoll（具体看下面的文章）
* poll和select基本相同 只是select的最大fd数量为1024（bitmap）
* epoll API由三个系统调用组成，epoll为什么有状态的，因为会保存连接的fd，和监听fd。
* epoll 在实际项目中更可取，因为poll每次需要fd传参 epoll在循环之前把连接fd放入内核，之后来一个连接加一个fd，不用每次传参


### select poll epoll 详解
1. 使用场景：设计一个高性能的网络服务器（一个服务器要处理多个客户端的请求）
2. 一开始我们会想到使用多线程，但是多线程存在`上下文切换` 消耗太大 而且为每个客户端创建一个线程，服务器端的线程资源很容易被耗光。 多进程消耗更大 
3. 考虑使用单线程 能够保证单线程处理a请求的消息 b的消息不丢失的原因是`dma技术` 这样单线程处理多个请求成为可行的 ❓这里可以再去了解一下dma 其实我觉得是数据会放到内核缓冲区里是消息不丢失的原因，当然可能是dma拿到内核缓冲区的，可以再去了解
4. ulimit -n 单个进程能够监视的文件描述符的数量存在最大限制 1024
5. [下面三种具体代码看这](https://devarea.com/linux-io-multiplexing-select-vs-poll-vs-epoll/#.XYD0TygzaUl)
6. [参考文章 中文的](https://segmentfault.com/a/1190000003063859) 
7. [讲解三种的优缺点 这里说了并不是epoll最好 看场合的 什么时候用](https://www.ulduzsoft.com/2014/01/select-poll-epoll-practical-difference-for-system-architects/)
8. [知乎一篇文章](https://mp.weixin.qq.com/s/YdIdoZ_yusVWza1PU7lWaw)

在select 之前的方案
```c
// 但是在用户态的循环里使用系统调用是非常不划算的
while(1) {
  for(fd <-- fdlist) {
    if(read(fd) != -1) { // 这里的read是非阻塞的
      doSomeThing();
    }
  }
}
```
#### SELECT
```c
int select(
    int nfds,
    fd_set *readfds,
    fd_set *writefds,
    fd_set *exceptfds,
    struct timeval *timeout);
// nfds:监控的文件描述符集里最大文件描述符加1
// readfds：监控有读数据到达文件描述符集合，传入传出参数
// writefds：监控写数据到达文件描述符集合，传入传出参数
// exceptfds：监控异常发生达文件描述符集合, 传入传出参数
// timeout：定时阻塞监控时间，3种情况
//  1.NULL，永远等下去
//  2.设置timeval，等待固定时间
//  3.设置timeval里时间均为0，检查描述字后立即返回，轮询
```
```python
# 一般情况的伪码描述 select 轮询的模式
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
有数据的话 
1. 会把传入的rset有数据的位 置为1 然后返回
2. 用户态再去循环判断哪一个rset被置位了 if判断之后进行read系统调用 然后处理事件 

select的优点
* 很古老的API `跨平台` 可移植性

**select的缺点**
1. select 调用需要(bitmap 最大限制为1024个fd)，需要拷贝一份到内核，高并发场景下这样的拷贝消耗的资源是惊人的。（可优化为不复制）
2. select 在内核层仍然是通过遍历的方式检查文件描述符的就绪状态，是个同步过程，只不过无系统调用切换上下文的开销。（内核层可优化为异步事件通知）
3. select 仅仅返回可读文件描述符的个数，具体哪个可读还是要用户自己遍历 复杂度O(n)。（可优化为只返回给用户就绪的文件描述符，无需用户做无效的遍历 O(k)）
4. 还有个缺点是 如果只有一个fd为900的文件描述符，内核仍需从头遍历bitmap，从0到900... 这很麻烦，而后面的poll不用，内核仅遍历传入的fds。

![select流程图](https://mmbiz.qpic.cn/mmbiz_png/GLeh42uInXTyY80RSpUTLjIMiaGGicv9zAr5qibfgLBad0zoCEWXxdqC9I4v4mAYLR2SiafwtG4qOmdicHxa1Sx8MKQ/640?wx_fmt=png&wxfrom=5&wx_lazy=1&wx_co=1)

#### POLL
```c
int poll (struct pollfd *fds, unsigned int nfds, int timeout);

struct pollfd {
      int fd;		 //连接的fd 文件描述符
      short events;  //关心的事件是什么 POLLIN--读 POLLOUT 写
      short revents; //内核对事件的回应 response
};
// POLLIN | POLLOUT 表示读写都关心 10 | 01 = 11
```
1. 如果fd中有数据 内核将pollfd.renvents |= POLLIN 也就是置位 然后poll返回
2. 用户态再重置revent
**poll的优缺点**
1. 去掉了 select 只能监听 1024 个文件描述符的限制。
2. 缺点和select一样

#### EPOLL [这里有个epoll内部的实现机制](http://mp.weixin.qq.com/s?__biz=MjM5Njg5NDgwNA==&mid=2247484905&idx=1&sn=a74ed5d7551c4fb80a8abe057405ea5e&chksm=a6e304d291948dc4fd7fe32498daaae715adb5f84ec761c31faf7a6310f4b595f95186647f12&scene=21#wechat_redirect)
```c
int epoll_create(int size);
int epoll_ctl(int epfd, int op, int fd, struct epoll_event *event);
int epoll_wait(int epfd, struct epoll_event *events, int max events, int timeout);
```
1. epoll_create(这个弹幕说会创建一个红黑树) epoll_ctl（添加连接fd到内核态） epoll_wait（返回所有的就绪fd） 
2. 我的理解是 epoll_create初始化一个红黑树 epoll_ctl把fd（n个）连接一个个放入内核中的fds（只需拷贝一次） 循环epoll_wait把内核中就绪了的fd（k个）返回给用户态
3. redis和nginx都是epoll实现
4. 当遇到大量的idle- connection，就会发现epoll的效率大大高于select/poll。 但是如果连接是短而多的，epoll不适用，因为每次连接到来都需要系统调用epoll_ctl() 添加新的fd

**epoll 优点**
1. 内核中保存一份文件描述符集合，无需用户每次都重新传入，只需告诉内核修改的部分即可。
2. 内核不再通过轮询的方式找到就绪的文件描述符，而是通过异步 IO 事件唤醒。
3. 内核仅会将有 IO 事件的文件描述符返回给用户，用户也无需遍历整个文件描述符集合。

####  TODO Q&A
复用了什么？？
* 在一个线程复用了多个客户端连接的等待状态

一次系统调用 + 内核层遍历这些文件描述符
* 这是IO多路复用的本质好处，给到内核态完成大量工作了，所以快

待办
* 去测试三种并发的连接数 或者类似于QPS的东西 可以网上找找有没有类似的


### 07 08 9.17
* 07主要是get set del 操作的实现 自定义了协议 难度不大
* 目前为止难度最大的就是06 event loop那里 事件循环的三种方式
* 据了解go net/http的 listen就是（socket bind listen accept）的集合 且linux的go内置epoll 
* TODO: 可以考虑用go写一个redis 然后对比c++测试

#### 08 疑问
为什么哈希表要用2的幂 而不用素数 ？？
* 对于 2 的幂，MOD 和按位 AND 输出相同的结果，并且考虑到按位 AND 比 MOD 快一个数量级 
* (i % 2^k) == (i & (2^k) - 1) 

那为什么不用素数减少碰撞
* 用2的幂还有没有别的好处 待探究

2的幂 位运算怎么判断 n & (n -1 ) ==0 因为为2的幂 只有最高位为1

使用位掩码进行索引操作的好处是，它可以在常数时间内完成，不需要进行昂贵的除法或取模运算。这对于大型的哈希表或需要高性能的应用非常重要。

> 对于哈希表的大小为8的情况，假设节点的哈希码为18，其二进制表示为 00010010。通过与位掩码 00000111 进行按位与运算，只保留了哈希码的低3位，得到的结果为 00000010，即索引为2。这样，节点就被插入到了哈希表的第2个位置。

* 当负载因子太高 需要扩容哈希表 但是扩容会耗费时间 可能redis不可用 需要保留两个哈希表 逐渐移动节点
* 哈希码hcode的生成方式可以稍微注意一下 和一些加密方式md5啥的好像是类似的
* Exercises: We typically grow the hash table when the `load factor` hits 0.5 and `shrink` when we hit 0.125. 官方答案是如果继续运行 扩容可能是不必要的 网上有到0.125时缩容
    > Besides, shrinking does not always return the memory to OS, this is dependent on many factors such as the malloc implementation and the level of memory fragmentation; the outcome of shrinking is not easily predictable.
* 建议周期性的缩容(例如20分钟 试探一次? 或者delete 多少多少次再缩容)
* 因为收缩的内存不一定返回给操作系统 这取决于malloc的实现和内存碎片化水平

#### ⭐⭐侵入式数据结构  这个困扰了一会 但是理解了之后感觉还是很有用的 而且只有没有gc的语言可以实现 (这可以写在简历上)
```c
// hashtable node, should be embedded into the payload
struct HNode {
    HNode *next = NULL;
    uint64_t hcode = 0;
};

// the structure for the key
struct Entry {
    struct HNode node;
    std::string key;
    std::string val;
};
```
也就是哈希表中不放任何数据,只放一个哈希码用于查找
但是next指针是和entry一起分配内存的,它们是连续的
可以靠next的地址拿到Entry的地址
>在C/C++中，new 和 malloc 单次分配的内存块是连续的。无论是使用 new 运算符还是 malloc 函数，它们都会向操作系统请求一段连续的内存空间来存储数据。

侵入式数据结构(`Intrusive Data Structures`)的优点
* 通用性 可以将同一实体用于不同的数据结构 改一下HNode即可
* 减少内存管理 不用为结点和数据分别分配内存(但是我感觉不嵌入 也可以啊)

```c
#define container_of(ptr, type, member) ({                  \
    const typeof( ((type *)0)->member ) *__mptr = (ptr);    \
    (type *)( (char *)__mptr - offsetof(type, member) );})
```
这一块也很难理解 首先传参 ptr , member 是HNode* 类型 type是Entry类型
((type *)0)->member 这一部分0强转为`type*`也就是`Entry*` 其实也就是NULL Entry
反正就是通过成员变量的指针 获取整个结构体变量的地址

({ statement1; statement2; ...; statementN; }) 这是语句表达式 值为statementN (最后一个语句)


### 10 11 AVL zset
* Exercises1: 这个AVL不是特别高效由于`some reductant pointer updates`
* 考虑存搞得差而不是存高度 这个以后再说吧 AVL工业界用的不多 
* Exercises2：构建AVL更多的测试用例
* 使用分析工具检查测试用例是否完全覆盖目标代码会很有帮助 和模糊测试
* TODO:优点改造成跳表  并构造详细的测试
* 记得做Exercises：实现zrank 命令 计算某个范围内的元素数量 请尝试添加更多命令
* 做完再继续下一节


### 突发奇想的疑问
系统调用的底层实现原理
* [系统调用的实现原理 感觉这文章写的还行](https://www.cnblogs.com/leekun/articles/2628339.html)  粗略的看了一下 还得继续看 不清晰 记得笔记

还有一个疑问 如果使用特定语言构建自己的框架或者中间件啥的 基础架构
是不是只使用（或者大部分使用）标准库的东西
* 也是可以引入别的库的 

### 工具的学习使用

#### ⭐linux 文本搜索
```shell
# -E 表示展开所有宏 grep -C 5 表示显示匹配行额上下各5行
g++ -E test_contain.cpp | grep -C 5 "(Entry*"
# ack: A search tool like grep, optimized for developers.
# 更多用法后续再探索把  awk和sed可以去了解 处理文本
ack -C 5 "Entry" test_contain.cpp
```

#### valgrind 检查内存泄漏
```shell
# 可以用tldr看看用法 或者man
valgrind --log-file=valReport --leak-check=full --show-reachable=yes --leak-resolution=low ./a.out
valgrind --tool=massif --stacks=yes ./a.out
massif-visualizer massif.out.15379
```

#### GDB 以字符串形式打印uint8指针
* `x/s` 内存中的数据解释为字符串并打印出来。以给定的地址开始，按照字符串的格式打印数据，直到遇到 null 终止符（'\0'）
* `x/4s buf` 打印4个字节 分开打印
* `x/4d buf` 以整数打印 没有d就是以 二进制（b） x（16进制） 
* `x/16c buf`
* p 命令（或 print 命令）：它是 GDB 中用于打印变量值的命令。p 命令将根据变量的类型解释内存中的数据，并打印出相应类型的值。它可以打印各种类型的变量，包括整数、浮点数、指针、结构体等。p 命令通常用于调试过程中观察和检查变量的值。
* info args
* info locals

`strace` 查看可执行程序使用的系统调用

### 面试可以说
* 实现了 poll epoll
* 设计了TLV协议09节
* 实现了基本的get set del操作
* 设计zset 分别用AVL 和skiplist（TODO） 实现
* 侵入式数据结构
* 使用valgrind优化程序 使用了makefile 构建C++程序 strace跟踪系统调用
* 对比测试不同实现的优劣（TODO）