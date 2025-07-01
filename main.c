#include <stdio.h>
#include <unistd.h>
#include <errno.h>
#include "rawmode.h"

int main() {
    enableRawMode();

    printf("进入原始模式，按'q'退出。");
    printf("\r\n按下任意键查看其ASCII码。\r\n");
    fflush(stdout);

    while (1) {
        char c;
        ssize_t nread = read(STDIN_FILENO, &c, 1);
        if (nread == -1 && errno != EAGAIN) die("read");
        if (nread == 0) continue;
        if (c == 'q') {
            printf("\r\n成功退出原始模式。\r\n");
            break;
        }
        
        printf("你按下的ASCII码: %d\r\n", c);
        fflush(stdout);
    }
}