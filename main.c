#include <stdio.h>
#include <unistd.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include "rawmode.h"
#include "editor.h"

#define MAX_INIT_LEN 1024

int printAsciiLoop(char *buf, int maxlen) {
    // 清屏并回到左上角
    write(STDOUT_FILENO, "\x1b[2J", 4);
    write(STDOUT_FILENO, "\x1b[H", 3);
    printf("进入原始模式，按下任意键查看其ASCII码。按'q'退出。\n");
    fflush(stdout);

    int len = 0;
    while (len < maxlen - 1) {
        //Linux系统直接读取键盘输入
        char c;
        ssize_t nread = read(STDIN_FILENO, &c, 1);//单字节设置为1
        if (nread == -1 && errno != EAGAIN) break;
        if (nread == 0) continue;


        printf("你按下的ASCII码: %d\r\n", (unsigned char)c);
        fflush(stdout);//强制刷新缓冲区

        if (c == 'q') {

            write(STDOUT_FILENO, "\x1b[2J", 4);
            write(STDOUT_FILENO, "\x1b[H", 3);

            printf("退出ASCII显示，进入编辑器...\n");
            fflush(stdout);
            break;
        }
        buf[len++] = c;
    }
    buf[len] = '\0';
    return len;
}

int main(int argc, char *argv[]) {
    enableRawMode();
    char *initbuf = NULL;
    int initlen = 0;
    char *filename = NULL;
    if (argc >= 2) {
        filename = argv[1];
        FILE *fp = fopen(argv[1], "r");//打开文件
        if (fp) {
            fseek(fp, 0, SEEK_END);//移动到文件末尾
            initlen = ftell(fp);//获取文件长度
            fseek(fp, 0, SEEK_SET);//
            initbuf = malloc(initlen + 1);
            if (initbuf) {
                fread(initbuf, 1, initlen, fp);
                initbuf[initlen] = '\0';
            }
            fclose(fp);
        }
    }
    //处理默认内容
    if (!initbuf) {
        char buf[MAX_INIT_LEN];
        initlen = printAsciiLoop(buf, MAX_INIT_LEN);
        if (initlen == 0) {
        strcpy(buf, "Hello, World!");
        initlen = strlen(buf);
    }

        initbuf = strdup(buf);//复制内容到initbuf
    }

    write(STDOUT_FILENO, "\x1b[2J", 4);
    write(STDOUT_FILENO, "\x1b[H", 3);

    editorProcessInputLoop(initbuf, initlen, filename ? filename : "");//主编辑循环

    free(initbuf);
    return 0;
}