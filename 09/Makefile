# 编译器设置
CC = g++
CFLAGS = -Wall -Wextra -g #Wall 开启所有警告 Wextra 开启额外的警告

# 目标文件名
TARGET = server
TARGET1 = client
# 源文件列表
SRCS = 09_server.cpp hashtable.cpp
SRCS1 = 09_client.cpp
# 生成的目标文件列表
OBJS = $(SRCS:.cpp=.o)
OBJS1 = $(SRCS1:.cpp=.o)
# 默认目标
all: $(TARGET) $(TARGET1)

# 生成目标文件
%.o: %.cpp
	$(CC) $(CFLAGS) -c $< -o $@  
# -o 好像只是用来表述目标文件的名称 -c没有链接  
# $@ 表示目标文件的名称，$< 表示第一个依赖的文件 
# $^ 表示所有依赖的文件

# 生成可执行文件
$(TARGET): $(OBJS)
	$(CC) $(CFLAGS) $^ -o $@

$(TARGET1): $(OBJS1)
	$(CC) $(CFLAGS) $^ -o $@

# 清理生成的文件
clean:
	rm -f $(OBJS) $(TARGET)
	rm -f $(OBJS1) $(TARGET1)