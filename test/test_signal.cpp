#include <fcntl.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#define BUFSIZE 1024

void handle_sigio(int signum)
{
    // 信号处理函数
    printf("Received SIGIO signal\n");
    // 处理异步 I/O 完成事件
    // ...
}

int main()
{
    int fd;
    char buffer[BUFSIZE];

    // 打开文件
    fd = open("valid.cpp", O_RDONLY);
    if (fd < 0) {
        perror("Error opening file");
        exit(1);
    }

    // 设置文件描述符的信号驱动I/O标志位
    fcntl(fd, F_SETOWN, getpid());
    fcntl(fd, F_SETFL, fcntl(fd, F_GETFL) | O_ASYNC);

    // 注册 SIGIO 信号处理函数
    struct sigaction sa;
    sa.sa_handler = handle_sigio;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = 0;
    sigaction(SIGIO, &sa, NULL);

    // 读取数据
    ssize_t bytes_read = read(fd, buffer, BUFSIZE);
    if (bytes_read < 0) {
        perror("Error reading file");
        exit(1);
    }
    write(2, &bytes_read, 8);
    // 等待异步 I/O 完成
    sleep(35);
    // 看到的现象是等了5秒再输出byte_read 但是没有调用信号处理函数
    // 关闭文件
    close(fd);

    return 0;
}