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
* 9.16 3个exercises已经完成 自己拿已有的客户端 验证了一下

### IO多路复用 select poll epoll
1. 使用场景：设计一个高性能的网络服务器（一个服务器要处理多个客户端的请求）
2. 一开始我们会想到使用多线程，但是多线程存在`上下文切换` 消耗太大`❓为什么不使用多进程和协程 这些是不是还分为用户态和内核态` 多线程CPU切换效率更慢
3. 考虑使用单线程 能够保证单线程处理a请求的消息 b的消息不丢失的原因是`dma技术` 这样单线程处理多个请求成为可行的 ❓这里可以再去了解一下dma
4. ulimit -n 单个进程能够监视的文件描述符的数量存在最大限制 1024
5. [下面三种具体代码看这](https://devarea.com/linux-io-multiplexing-select-vs-poll-vs-epoll/#.XYD0TygzaUl)
6. [参考文章 中文的](https://segmentfault.com/a/1190000003063859)
7. [可参考的文章 讲解三种的优缺点 什么时候用](https://www.ulduzsoft.com/2014/01/select-poll-epoll-practical-difference-for-system-architects/)
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


### 07 08 9.17
* 07主要是get set del 操作的实现 自定义了协议 难度不大
* 目前为止难度最大的就是06 event loop那里 事件循环的三种方式
* 据了解go net listen就是（socket bind listen accept）的集合 且linux内置epoll 可以考虑用go写一个redis 然后

#### 08 提问
* 为什么哈希表要用2的幂 而不用素数 ？？
* 对于 2 的幂，MOD 和按位 AND 输出相同的结果，并且考虑到按位 AND 比 MOD 快一个数量级  (i % 2^k) == (i & (2^k) - 1) 那为什么不用素数减少碰撞
* 2的幂位运算怎么判断 n & (n -1 ) ==0 因为为2的幂 只有最高位为1
* 使用位掩码进行索引操作的好处是，它可以在常数时间内完成，不需要进行昂贵的除法或取模运算。这对于大型的哈希表或需要高性能的应用非常重要。
* 插入 查找 删除 书里指针这一块太太绕了 指针的指针
> 对于哈希表的大小为8的情况，假设节点的哈希码为18，其二进制表示为 00010010。通过与位掩码 00000111 进行按位与运算，只保留了哈希码的低3位，得到的结果为 00000010，即索引为2。这样，节点就被插入到了哈希表的第2个位置。

* 当负载因子太高 需要扩容哈希表 但是扩容会耗费事件 可能redis不可用 需要保留两个哈希表 逐渐移动节点
* 08节页太难了吧 很多都看不懂
* 哈希码hcode的生成方式可以稍微注意一下 和一些加密方式md5啥的好像类似模式
* Exercises: We typically grow the hash table when the `load factor` hits 0.5 and `shrink` when we hit 0.125. 官方答案是如果继续运行 扩容可能是不必要的 网上有到0.125时缩容
    * >Besides, shrinking does not always return the memory to OS, this is dependent on many factors such as the malloc implementation and the level of memory fragmentation; the outcome of shrinking is not easily predictable.
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


### GDB 以字符串形式打印uint8指针
* `x/s` 内存中的数据解释为字符串并打印出来。以给定的地址开始，按照字符串的格式打印数据，直到遇到 null 终止符（'\0'）
* `x/4s buf` 打印4个字节 分开打印
* `x/4d buf` 以整数打印 没有d就是以 二进制（b） x（16进制） 
* `x/16c buf`
* p 命令（或 print 命令）：它是 GDB 中用于打印变量值的命令。p 命令将根据变量的类型解释内存中的数据，并打印出相应类型的值。它可以打印各种类型的变量，包括整数、浮点数、指针、结构体等。p 命令通常用于调试过程中观察和检查变量的值。
* info args
* info locals

### 面试可以说
* 实现了 poll epoll
* 设计了协议09节
* 实现了基本的get set del操作
* 设计zset 分别用AVL 和skiplist（TODO） 实现