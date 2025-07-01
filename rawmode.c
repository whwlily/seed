#include<stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <termios.h>
#include <errno.h>

struct termios orig_termios;

// 错误处理函数
void die(const char *s) {
    perror(s);
    exit(1);
}

// 恢复终端设置
void disableRawMode() {
    if (tcsetattr(STDIN_FILENO, TCSAFLUSH, &orig_termios) == -1) {
        write(STDOUT_FILENO, "无法恢复终端设置\n", 24);
    }
}

// 启用原始模式
void enableRawMode() {
    //获取当前终端的属性并且进行判定
    if (tcgetattr(STDIN_FILENO, &orig_termios) == -1) die("tcgetattr");
    //注册函数确保终端能被恢复
    atexit(disableRawMode);

    //按照要求设置 根据termios结构体的标准一一对应
    struct termios raw = orig_termios;
    raw.c_iflag &= ~(BRKINT | ICRNL | INPCK | ISTRIP | IXON);
    raw.c_oflag &= ~(OPOST);
    raw.c_cflag |= (CS8);
    raw.c_lflag &= ~(ECHO | ICANON | ISIG | IEXTEN);
    raw.c_cc[VMIN] = 0;
    raw.c_cc[VTIME] = 10; // 1秒超时

    //成功则设置终端属性，失败则使用die函数输出错误信息，比较公式化
    if (tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw) == -1) die("tcsetattr");
}
