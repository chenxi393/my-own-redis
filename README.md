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
* 减少内存管理 不用为结点和数据分别分配内存(但是我感觉不嵌入 也可以啊--其实11.zset.h 那里就体现了 因为有续集要使用多种数据结构)

```c
#define container_of(ptr, type, member) ({                  \
    const typeof( ((type *)0)->member ) *__mptr = (ptr);    \
    (type *)( (char *)__mptr - offsetof(type, member) );})
```
这一块也很难理解 首先传参 ptr , member 是HNode* 类型 type是Entry类型
((type *)0)->member 这一部分0强转为`type*`也就是`Entry*` 其实也就是NULL Entry
反正就是通过成员变量的指针 获取整个结构体变量的地址

({ statement1; statement2; ...; statementN; }) 这是语句表达式 值为statementN (最后一个语句)


### 10 11 AVL zset  TODO
* Exercises1: 这个AVL不是特别高效由于`some reductant pointer updates`
* 考虑存搞得差而不是存高度 这个以后再说吧 AVL工业界用的不多 
* Exercises2：构建AVL更多的测试用例
* 使用分析工具检查测试用例是否完全覆盖目标代码会很有帮助 和模糊测试
* TODO:优点改造成跳表  并构造详细的测试
* 记得做Exercises：实现zrank（给键查找排名） 命令 计算某个范围内的元素数量 请尝试添加更多命令 TODO TODO 9.24 先放着 实现跳表再一起实现
* 做完再继续下一节

### skiplist zset TODO
redis的跳表比原始论文描述的算法做了以下几点改动
1. 允许重复分数
2. 键相等时，继续比较结点数据
3. 双向链表实现 （便于实现ZREVRANGE 或 ZREVRANGEBYSCORE 命令）

面试常问：为什么选择用跳表而不是其他有序的数据结构（红黑树，AVL等）
* 哈希表不是有序的 不适用范围查找（zset经常用于访问查找 排名场景经常用） 
* 平衡树 范围查找需要找到最小值 再以中序遍历继续找（不好实现）
* 而跳表只需要找到最小值 遍历最下一层即可，`易于范围查找`
* 平衡树插入删除的逻辑复杂，而且别的操作 跳表`易于实现,调试,修改`
* `内存占用少` p=1/4 时 跳表每个节点只包含1.33个指针 而平衡树每个节点需要2个指针

基本结构
* 表头：维护跳跃表各层的指针。（默认32层）
* 中间节点：维护节点数据和各层的前进后退指针。
* 层：保存指向该层下一个节点的指针和与下个节点的间隔（span）。为了提高查找效率，程序总是先从高层开始访问，然后随着范围的缩小慢慢降低层次。
* 表尾：全部为NULL，表示跳跃表各层的末尾。

具体实现细节见代码注释
```cpp
struct zskiplistNode {
    std::string* obj;   // 这里改为string
    double score;       //按分值从小到大来排序
    // 后退指针，只有第0层有效   逆序操作会用到
    zskiplistNode* backward;
    // 各层的前进指针及与下一个节点的间隔
    struct zskiplistLevel {
        zskiplistNode* forward; 
        unsigned int span; // 跨度实际上是用来计算排位的
    } level[];
    // 实际上 结点数量就是元素数量+1 有头结点
    // 每个结点可能会有很多层
    // 而头结点默认32层
};

struct zskiplist {
    // 表头节点和表尾节点
    zskiplistNode *header, *tail;
    // 表中节点的数量
    unsigned long length;
    // 表中层数最大的节点的层数
    int level;
};
```
[这篇不错](http://zhangtielei.com/posts/blog-redis-skiplist.html)
1. ⭐⭐文中说 新结点的层高level是概率生成的 为什么？？ 幂次定律又是什么
    * 若原来的底层结点与上层为2：1的关系（保证logn 的查找时间），新插入结点会打破2:1 若维护2：1的关系，则需要将插入节点位置后面所有的节点重新调整，这个时间是O（n）的，删除同理
    * 所以我们不能要求上层节点个数是下层的一半。
    * 解决办法就是随机生成每个节点的level （⭐⭐这是为什么插入要优于AVL）
    * 由以下伪码根据概率论公式可以计算一个节点平均的层数（期望）E = 1 / (1-p)
    * 当p=1/4时，每个节点所包含的平均指针数目为1.33
    * 有机会可以看原始论文的计算方式 上面文中计算的平均时间复杂度为O(log n)
```python
# 计算随机层数的伪码描述
randomLevel()
    level := 1
    // random()返回一个[0...1)的随机数
    while random() < p and level < MaxLevel do
        level := level + 1
    return level
# 在redis 中 
    p = 1/4
    MaxLevel = 32
```

2. 前进指针 forward 用来遍历skiplist
3. 跨度 span 实际上是用来计算排位的，无论哪一层查找，将沿途访问过的结点的跨度累计就是排位
4. backward 后退指针只有第0层有效 只能一个个倒退
5. 排序按照score(可以重复) 和obj（member）的字典序 一起排序
6. zset排名是从0开始计算 从大到小 字典序在前

实际上当数据量小的时候zset是由ziplist（TODO:需要去了解）实现的
下面两个配置配置了何时换成skiplist
```python
zset-max-ziplist-entries 128
zset-max-ziplist-value 64
```

### 12 事件循环和计时器  TODO
1. IO操作和网络操作都需要超时设置
    * 超时强踢 不能一直占着TCP连接不用
2. 首先考虑TCP连接的超时，考虑使用链表，新的连接放到末尾即可 这个链表只能实现固定时间（给每一个连接的的闲时是固定的 要想不固定得用排序结构）
3. 看了一下实现 也是使用侵入式数据结构
4. 文章一堆static的意思是函数或者变量作用域仅限于当前文件
5. 作业1 为IO操作添加超时操作 疑问 这里的IO操作不都是非阻塞的嘛 而且不是 内核告诉我们有数据了才会去read 和write 难道是处理一次之后的 因为一次请求后客户端会主动关闭连接
6. 作业2 是用排序结构（堆）实现计时器 TODO TODO 这些先待办把 最后再来实现

### 13 堆和TTL TODO
1. 缓存需要设置TTL(time to live)
2. 采用最小堆（即根最小） 
3. 每次来新元素放到数组最后一个 然后逐级调整（值变动 大放子节点 小放父节点 递归/递推））
4. 作业1： 堆的add操作 需要logn的时间  怎么优化（使用哈希表喽）
    * 答案说使用n叉树 比如4叉树 但实现起来会不会复杂
    * 定时器不必非常精确的存储 可以把差别不大的键放在一起 但是获取键的时候给出精确的查询时间 但其实还是log时间复杂度的 快一些些
5. 作业2： Redis真实的操作 没有使用排序结构实现过期
    * 实际上 redis键的过期是放在过期字典里的 删除方式为惰性删除 和定期删除  没有采用定时删除（也就是到了时间就删除）

### 14 线程池和异步任务 TODO
1. 存在的问题 sorted set 删除键 在集合很大的情况下 删除可能很慢（logn） 因为命令的执行是单线程的嘛 键销毁期间服务是不可用的
2. 考虑使用多线程 另开一个线程运行析构函数（键销毁函数）--似乎redis本身就是这么实现的（具体可以看看源码 再去了解）
3. 使用pthread API (之前操作系统刚好学过) `pthread_mutex_t` （互斥锁） `pthread_cond_t` （条件变量）
4. consumer在队列空的时候休眠 不空时使用条件变量唤醒
5. 由mutex 保护队列的访问 这很操作系统（非常经典的使用）
6. 还有个问题就是 如果异步删除的时候 客户端访问删除的键怎么办(代码里好像没有保护 join肯定没有意义 那为什么不主线程呢)
7. 作业1： 尝试用信号量（替换条件变量和互斥锁）来实现线程池 TODO
    * 信号量操作系统实验2有（用作条件变量 用做锁） 但是有点久远快忘了
8. 作业2： 
    * Implement the mutex using the semaphore. (Trivial)
    * Implement the semaphore using the condition variable. (Easy)
    * Implement the condition variable using only mutexes. (Intermediate)
    * Now that you know these primitives are somewhat equivalent, why should you prefer one to another?


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


### 04 协议设计 09 序列化设计（编解码）
TCP没有边界
```m
+-----+------+-----+------+--------
| len | msg1 | len | msg2 | more...
+-----+------+-----+------+--------
```
设计简单的应用层协议

len为4字节固定的小端整数

文本 VS 二进制
* 文本人类可读 容易调试 HTTP1就是
* 二进制简单 不容易出错 也就是定长的

HTTP协议的分块传输编码
```http
POST /upload HTTP/1.1
Host: example.com
Transfer-Encoding: chunked

5
Hello
6
 World
0
```

使用特殊字符 分隔符指示流的结束

09 TLV序列化方案

1个字节 表示类型（Type）

4个字节表示长度（Length） 定长类型可以没有省略
 

### TODO
* 实现了 poll epoll epoll有两种触发方式  ET 边缘触发和LT条件触发
* 设计了TLV协议09节
* 实现了基本的get set del操作
* 设计zset 分别用AVL 和skiplist（TODO） 实现
* 侵入式数据结构
* 使用valgrind优化程序 使用了makefile 构建C++程序 strace跟踪系统调用 python脚本测试程序
* 对比测试不同实现的优劣（TODO）
* 这个项目非常好 主要是数据结构 计算机网络和操作系统也有涉及
* 我觉得可以学以致用
* 尝试给作者发一封邮件（有两个疑问 1. 12节为什么要为IO操作添加超时操作，内核告诉有数据才取读写吗 2. 还有一个问题就是异步删除，这时候主线程被询问还未来得及删除的元素怎么办 可以自己验证一下 会不会有这种问题） 再研究研究 再去问问题
* 既然 客户端和网络端是网络传输 那能不能改成零拷贝的方式 并且验证优点 (感觉有些零拷贝方式不行 因为用户态需要拿到数据处理 而不能放在内核态)

TODO: 更多数据类型，RDB，AOF持久化，考虑对比使用Go重构