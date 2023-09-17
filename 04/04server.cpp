#include <assert.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <errno.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <netinet/ip.h>


static void msg(const char *msg) {
    fprintf(stderr, "%s\n", msg);
}

static void die(const char *msg) {
    int err = errno;
    fprintf(stderr, "[%d] %s\n", err, msg);
    abort();
}

const size_t k_max_msg = 4096;

/*⭐⭐⭐
> Our server will be able to process multiple requests from a client, 
so we need to implement some sort of “protocol” to 
at least split requests apart from the TCP byte stream.
+-----+------+-----+------+--------
| len | msg1 | len | msg2 | more...
+-----+------+-----+------+--------
这里read_full的意思是说客户端可能会一次发送多个请求
而tcp是面向连接的字节流 一次读取并不知道要读多少个
所以设计简单的协议 规定每次先读四个字节 表示长度
在读相应长度的字节
确保读到每一个请求（也就是上图的msg）

⭐⭐⭐协议的设计
1. 文本还是二进制协议 HTTP是文本协议 优点是人类可读 文本协议复杂 难以解析
2. 文本解析涉及长度计算 边界检查 决策难
3. 文本协议设计技巧：避免不必要的可变长度组件 考虑固定长度
4. 我们的协议首部显式表面消息的长度，但现实中HTTP（Chunked encoding）分块编码
5. 用特殊消息指示流结束 还有一种不太好的方法 用特殊字符（分隔符）缺点是消息内部需要转义特殊字符
*/

/* ⭐⭐ TODO 实验缓存IO减少系统调用的次数  
read as much as you can into a buffer at once, 
then try to parse multiple requests from that buffer. 
*/
static int32_t read_full(int fd, char *buf, size_t n) {
    while (n > 0) {
        // ⭐read 和 write 系统调用会返回读入或写入的字节数 一定要处理返回值
        // blocks if there is none 内核缓冲区没有数据则阻塞 
        // 可能读不到预期那么多的数据 根据返回值处理数据不足的问题 
        // read write系统调用是字节流而不是消息！！！
        ssize_t rv = read(fd, buf, n);
        if (rv <= 0) {
            return -1;  // error, or unexpected EOF
        }
        assert((size_t)rv <= n);// assert内为假 触发assert 崩溃程序
        n -= (size_t)rv;
        buf += rv;
    }
    return 0;
}

static int32_t write_all(int fd, const char *buf, size_t n) {
    while (n > 0) {
        // write和上面同理 会遇到内核缓冲区已满的情况
        ssize_t rv = write(fd, buf, n);
        if (rv <= 0) {
            return -1;  // error
        }
        assert((size_t)rv <= n);
        n -= (size_t)rv;
        buf += rv;
    }
    return 0;
}

static int32_t one_request(int connfd) {
    // 4 bytes header
    char rbuf[4 + k_max_msg + 1];
    errno = 0;
    int32_t err = read_full(connfd, rbuf, 4);
    if (err) {
        if (errno == 0) {
            msg("EOF");
        } else {
            msg("read() error");
        }
        return err;
    }

    uint32_t len = 0;
    memcpy(&len, rbuf, 4);  // assume little endian ⭐vaild.cpp验证了小段存储
    if (len > k_max_msg) {// 控制最大的消息数
        msg("too long");
        return -1;
    }

    // request body
    err = read_full(connfd, &rbuf[4], len);
    if (err) {
        msg("read() error");
        return err;
    }

    // do something
    rbuf[4 + len] = '\0';
    printf("client says: %s\n", &rbuf[4]);

    // reply using the same protocol
    const char reply[] = "world";
    char wbuf[4 + sizeof(reply)];
    len = (uint32_t)strlen(reply);
    memcpy(wbuf, &len, 4);
    memcpy(&wbuf[4], reply, len);
    return write_all(connfd, wbuf, 4 + len);
}

int main() {
    // ⭐ fd引用了Linux内核的某些内容 TCP连接 磁盘文件等 这里显然是TCP连接
    // ⭐ AF_INET is for IPv4, use AF_INET6 for IPv6 or dual-stack socket
    // SOCK_STREAM is for TCP
    int fd = socket(AF_INET, SOCK_STREAM, 0);
    if (fd < 0) { //连接失败abort程序
        die("socket()");
    }

    // this is needed for most server applications
    // ⭐⭐Without SO_REUSEADDR option, 
    // the server won’t able to bind to the same address if restarted.
    // 测试可知 这个选项很有用 若没设置这个选项 与客户端建立连接（accept）后
    // 重启程序后bind() 会失败 必然发生 已经测试过
    int val = 1;
    setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &val, sizeof(val));

    // bind   ⭐bind将地址与fd关联起来
    struct sockaddr_in addr = {};
    addr.sin_family = AF_INET;
    addr.sin_port = ntohs(1234);
    addr.sin_addr.s_addr = ntohl(0);    // wildcard address 0.0.0.0
    int rv = bind(fd, (const sockaddr *)&addr, sizeof(addr));
    if (rv) {
        die("bind()");
    }

    // listen ⭐监听该地址
    rv = listen(fd, SOMAXCONN);
    if (rv) {
        die("listen()");
    }

    while (true) {
        // accept ⭐当建立起连接时 accept返回一个代表了连接套接字的fd
        struct sockaddr_in client_addr = {};
        socklen_t socklen = sizeof(client_addr);
        int connfd = accept(fd, (struct sockaddr *)&client_addr, &socklen);
        if (connfd < 0) {
            continue;   // error
        }
        /*
        在应用层，需要设计合适的协议或消息格式来将请求分割成独立的消息，
        以便服务器能够正确解析和处理它们。
        这样可以确保多个客户端的请求在服务器端得到独立处理。
        */
        while (true) {
            // here the server only serves one client connection at once
            // ⭐一次只能处理一个客户端的请求
            // 05节则说如何更改为并发
            int32_t err = one_request(connfd);
            if (err) {
                break;
            }
        }
        close(connfd);
    }

    return 0;
}