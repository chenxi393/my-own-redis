#include <arpa/inet.h>
#include <assert.h>
#include <errno.h>
#include <fcntl.h>
#include <netinet/ip.h>
#include <poll.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/epoll.h>
#include <sys/socket.h>
#include <unistd.h>
#include <vector>

static void msg(const char* msg)
{
    fprintf(stderr, "%s\n", msg);
}

static void die(const char* msg)
{
    int err = errno;
    fprintf(stderr, "[%d] %s\n", err, msg);
    abort();
}

static void fd_set_nb(int fd)
{
    errno = 0;
    int flags = fcntl(fd, F_GETFL, 0);
    if (errno) {
        die("fcntl error");
        return;
    }

    flags |= O_NONBLOCK;

    errno = 0;
    (void)fcntl(fd, F_SETFL, flags);
    if (errno) {
        die("fcntl error");
    }
}

const size_t k_max_msg = 4096;
int epfd;
enum {
    STATE_REQ = 0,
    STATE_RES = 1,
    STATE_END = 2, // mark the connection for deletion
};

struct Conn {
    int fd = -1;
    uint32_t state = 0; // either STATE_REQ or STATE_RES
    // buffer for reading
    size_t rbuf_size = 0;
    size_t rbuf_remain = 0; // 改造memmove
    uint8_t rbuf[4 + k_max_msg];
    // buffer for writing
    size_t wbuf_size = 0;
    size_t wbuf_sent = 0;
    uint8_t wbuf[4 + k_max_msg];
};

static void conn_put(std::vector<Conn*>& fd2conn, struct Conn* conn)
{
    if (fd2conn.size() <= (size_t)conn->fd) {
        fd2conn.resize(conn->fd + 1);
    }
    fd2conn[conn->fd] = conn;
}

static int32_t accept_new_conn(std::vector<Conn*>& fd2conn, int fd)
{
    // accept
    struct sockaddr_in client_addr = {};
    socklen_t socklen = sizeof(client_addr);
    int connfd = accept(fd, (struct sockaddr*)&client_addr, &socklen);
    if (connfd < 0) {
        msg("accept() error");
        return -1; // error
    }

    // set the new connection fd to nonblocking mode
    fd_set_nb(connfd);
    // creating the struct Conn
    struct Conn* conn = (struct Conn*)malloc(sizeof(struct Conn));
    if (!conn) {
        close(connfd);
        return -1;
    }
    conn->fd = connfd;
    conn->state = STATE_REQ;
    conn->rbuf_size = 0;
    conn->wbuf_size = 0;
    conn->wbuf_sent = 0;
    conn_put(fd2conn, conn);
    // ⭐改造1
    static struct epoll_event ev;
    ev.data.fd = conn->fd;
    ev.events = (conn->state == STATE_REQ) ? EPOLLIN : EPOLLOUT;
    epoll_ctl(epfd, EPOLL_CTL_ADD, ev.data.fd, &ev);
    return 0;
}

static void state_req(Conn* conn);
static void state_res(Conn* conn);

static bool try_one_request(Conn* conn)
{
    // try to parse a request from the buffer
    if (conn->rbuf_remain < 4) {
        // not enough data in the buffer. Will retry in the next iteration
        return false;
    }
    uint32_t len = 0;
    memcpy(&len, &conn->rbuf[conn->rbuf_size - conn->rbuf_remain], 4);
    if (len > k_max_msg) {
        msg("too long");
        conn->state = STATE_END;
        return false;
    }
    if (4 + len > conn->rbuf_remain) {
        // not enough data in the buffer. Will retry in the next iteration
        return false;
    }

    // got one request, do something with it
    printf("client says: %.*s\n", len, &conn->rbuf[conn->rbuf_size - conn->rbuf_remain + 4]);

    if (conn->wbuf_size + 4 + len >= 4 + k_max_msg) {
        // 写入缓冲区可能已满
        conn->state = STATE_RES;
        state_res(conn);
    }
    // generating echoing response
    memcpy(&conn->wbuf[conn->wbuf_size], &len, 4);
    memcpy(&conn->wbuf[4 + conn->wbuf_size], &conn->rbuf[conn->rbuf_size - conn->rbuf_remain + 4], len);
    conn->wbuf_size += 4 + len;
    // remain
    conn->rbuf_remain = conn->rbuf_remain - 4 - len;
    // change state
    // conn->state = STATE_RES;
    // 06 exercise 3的意思是 这里不去 改变状态机 立马flush 缓冲区 而是再来一个相应
    // state_res(conn);

    // continue the outer loop if the request was fully processed
    return (conn->state == STATE_REQ);
}

static bool try_fill_buffer(Conn* conn)
{
    // try to fill the buffer
    assert(conn->rbuf_size < sizeof(conn->rbuf));
    ssize_t rv = 0;
    do {
        size_t cap = sizeof(conn->rbuf) - conn->rbuf_size;
        rv = read(conn->fd, &conn->rbuf[conn->rbuf_size], cap);
    } while (rv < 0 && errno == EINTR);
    // The EINTR means the syscall was interrupted by a signal,
    // the retrying is needed even if our application does not make use of signals.
    if (rv < 0 && errno == EAGAIN) {
        // got EAGAIN, stop.
        return false;
    }
    if (rv < 0) {
        msg("read() error");
        conn->state = STATE_END;
        return false;
    }
    if (rv == 0) {
        if (conn->rbuf_size > 0) {
            msg("unexpected EOF");
        } else {
            msg("EOF");
        }
        conn->state = STATE_END;
        return false;
    }

    conn->rbuf_size += (size_t)rv;
    assert(conn->rbuf_size <= sizeof(conn->rbuf));
    conn->rbuf_remain = conn->rbuf_size;
    // 表明缓冲区满 需要先处理再接着循环读取
    // Try to process requests one by one.
    // Why is there a loop? Please read the explanation of "pipelining".
    // clients can save some latency by sending multiple requests without waiting for responses
    while (try_one_request(conn)) { }
    // 多次请求 一次刷新
    conn->state = STATE_RES;
    state_res(conn);

    // remove muti request from the buffer.
    if (conn->rbuf_remain) {
        memmove(conn->rbuf, &conn->rbuf[conn->rbuf_size - conn->rbuf_remain], conn->rbuf_remain);
    }
    conn->rbuf_size = conn->rbuf_remain;

    return (conn->state == STATE_REQ);
}

static void state_req(Conn* conn)
{
    while (try_fill_buffer(conn)) { }
}

static bool try_flush_buffer(Conn* conn)
{
    ssize_t rv = 0;
    do {
        size_t remain = conn->wbuf_size - conn->wbuf_sent;
        rv = write(conn->fd, &conn->wbuf[conn->wbuf_sent], remain);
    } while (rv < 0 && errno == EINTR);
    if (rv < 0 && errno == EAGAIN) {
        // got EAGAIN, stop.
        return false;
    }
    if (rv < 0) {
        msg("write() error");
        conn->state = STATE_END;
        return false;
    }
    conn->wbuf_sent += (size_t)rv;
    assert(conn->wbuf_sent <= conn->wbuf_size);
    if (conn->wbuf_sent == conn->wbuf_size) {
        // response was fully sent, change state back
        conn->state = STATE_REQ;
        conn->wbuf_sent = 0;
        conn->wbuf_size = 0;
        return false;
    }
    // still got some data in wbuf, could try to write again
    return true;
}

static void state_res(Conn* conn)
{
    while (try_flush_buffer(conn)) { }
}

static void connection_io(Conn* conn)
{
    if (conn->state == STATE_REQ) {
        state_req(conn);
    } else if (conn->state == STATE_RES) {
        state_res(conn);
    } else {
        assert(0); // not expected
    }
}

int main()
{
    int fd = socket(AF_INET, SOCK_STREAM, 0);
    if (fd < 0) {
        die("socket()");
    }

    int val = 1;
    setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &val, sizeof(val));

    // bind
    struct sockaddr_in addr = {};
    addr.sin_family = AF_INET;
    addr.sin_port = ntohs(1234);
    addr.sin_addr.s_addr = ntohl(0); // wildcard address 0.0.0.0
    int rv = bind(fd, (const sockaddr*)&addr, sizeof(addr));
    if (rv) {
        die("bind()");
    }

    // listen
    rv = listen(fd, SOMAXCONN);
    if (rv) {
        die("listen()");
    }

    // a map of all client connections, keyed by fd
    std::vector<Conn*> fd2conn;

    // set the listen fd to nonblocking mode
    fd_set_nb(fd);

    // the event loop
    struct epoll_event events[4096]; // 定义最大连接数4096
    epfd = epoll_create(100);
    // for convenience, the listening fd is put in the first position
    static struct epoll_event pev;
    pev.data.fd = fd;
    pev.events = EPOLLIN;
    epoll_ctl(epfd, EPOLL_CTL_ADD, pev.data.fd, &pev);
    while (true) {
        // epoll for active fds
        // the timeout argument doesn't matter here
        int rv = epoll_wait(epfd, events, 4096, 1000);
        if (rv < 0) {
            die("epoll");
        }

        // process active connections
        for (int i = 0; i < rv; ++i) {
            if (events[i].data.fd != fd) {
                Conn* conn = fd2conn[events[i].data.fd];
                connection_io(conn);
                if (conn->state == STATE_END) {
                    // client closed normally, or something bad happened.
                    // destroy this connection
                    fd2conn[conn->fd] = NULL;
                    static struct epoll_event ev;
                    ev.data.fd = fd;
                    ev.events = EPOLLIN;
                    epoll_ctl(epfd, EPOLL_CTL_ADD, ev.data.fd, &ev);
                    (void)close(conn->fd);
                    free(conn);
                }
            } else {
                // try to accept a new connection if the listening fd is active
                (void)accept_new_conn(fd2conn, fd);
            }
        }
    }
    return 0;
}