#include <unistd.h>
#include <stdio.h>

/*⭐
管道是一种用于进程间通信的机制，可以在父进程和子进程之间传递数据。
在Linux中，管道分为匿名管道和有名管道两种形式。
下面的是匿名管道
*/
int main() {
    int fd[2];
    char buffer[20];
    pipe(fd); // 创建管道

    if (fork() == 0) {
        close(fd[0]); // 关闭读取端
        write(fd[1], "Hello, pipe!", 13); // 子进程写入数据到管道
        close(fd[1]); // 关闭写入端
    } else {
        close(fd[1]); // 关闭写入端
        read(fd[0], buffer, sizeof(buffer)); // 父进程从管道读取数据
        printf("Received: %s\n", buffer);
        close(fd[0]); // 关闭读取端
    }

    return 0;
}