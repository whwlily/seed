#include <stdio.h>
#include <unistd.h>
#include <errno.h>
#include "rawmode.h"
#include "editor.h"

#define MAX_INIT_LEN 1024

int printAsciiLoop(char *buf, int maxlen) {
    printf("进入原始模式，按'q'退出。\n");
    printf("按下任意键查看其ASCII码。\n");
    fflush(stdout);

    int len = 0;
    while (len < maxlen - 1) {
        char c;
        ssize_t nread = read(STDIN_FILENO, &c, 1);
        if (nread == -1 && errno != EAGAIN) break;
        if (nread == 0) continue;
        // 保证ASCII码输出不会刷到屏幕底部
        printf("你按下的ASCII码: %d\r\n", (unsigned char)c);
        fflush(stdout);
        if (c == 'q') {
            printf("退出ASCII显示，进入编辑器...\n");
            fflush(stdout);
            break;
        }
        buf[len++] = c;
    }
    buf[len] = '\0';
    return len;
}

int main() {
    enableRawMode();
    char initbuf[MAX_INIT_LEN];
    int initlen = printAsciiLoop(initbuf, MAX_INIT_LEN);

    // 清屏并回到左上角，避免ASCII打印残留
    write(STDOUT_FILENO, "\x1b[2J", 4);
    write(STDOUT_FILENO, "\x1b[H", 3);

    editorProcessInputLoop(initbuf, initlen);
    return 0;
}