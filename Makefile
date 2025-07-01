# 定义编译器和编译选项
CC = gcc 
CFLAGS = -Wall -Wextra -g  # 启用警告和调试信息
 
# 默认目标：生成 main.o
all: main.o
 
# 生成 main.o 的规则
main.o: main.c
	$(CC) $(CFLAGS) -c main.c -o main.o
 
# 清理生成的文件
clean:
	rm -f main.o 