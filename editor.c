#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include "editor.h"

#define ABUF_INIT {NULL, 0}
#define MAX_ROWS 100
#define MAX_COLS 256

enum editorKey {
    ARROW_LEFT = 1000,
    ARROW_RIGHT,
    ARROW_UP,
    ARROW_DOWN,
    HOME_KEY,
    END_KEY,
    PAGE_UP,
    PAGE_DOWN
};

struct abuf {
    char *b;
    int len;
};

static void abAppend(struct abuf *ab, const char *s, int len) {
    char *new_b = realloc(ab->b, ab->len + len);
    if (new_b == NULL) return;
    memcpy(&new_b[ab->len], s, len);
    ab->b = new_b;
    ab->len += len;
}

static void abFree(struct abuf *ab) {
    free(ab->b);
}

static void die(const char *s) {
    perror(s);
    exit(1);
}

// 多行文本缓冲区
static char lines[MAX_ROWS][MAX_COLS] = {{0}};
static int line_len[MAX_ROWS] = {0};
static int total_lines = 1;

// 光标位置
static int cx = 0;
static int cy = 0;

// 新增：屏幕滚动偏移
static int row_offset = 0;

// 读取一个按键，处理转义序列
static int editorReadKey() {
    char c;
    while (1) {
        ssize_t nread = read(STDIN_FILENO, &c, 1);
        if (nread == -1 && errno != EAGAIN) die("read");
        if (nread == 0) continue;
        break;
    }

    if (c == '\x1b') {
        char seq[3];
        if (read(STDIN_FILENO, &seq[0], 1) != 1) return '\x1b';
        if (read(STDIN_FILENO, &seq[1], 1) != 1) return '\x1b';

        if (seq[0] == '[') {
            if (seq[1] >= '0' && seq[1] <= '9') {
                if (read(STDIN_FILENO, &seq[2], 1) != 1) return '\x1b';
                if (seq[2] == '~') {
                    switch (seq[1]) {
                        case '1': return HOME_KEY;
                        case '3': return 127;
                        case '4': return END_KEY;
                        case '5': return PAGE_UP;
                        case '6': return PAGE_DOWN;
                        case '7': return HOME_KEY;
                        case '8': return END_KEY;
                    }
                }
            } else {
                switch (seq[1]) {
                    case 'A': return ARROW_UP;
                    case 'B': return ARROW_DOWN;
                    case 'C': return ARROW_RIGHT;
                    case 'D': return ARROW_LEFT;
                    case 'H': return HOME_KEY;
                    case 'F': return END_KEY;
                }
            }
        }
        return '\x1b';
    } else {
        return c;
    }
}

// 获取终端窗口大小（简化版）
static int getWindowSize(int *rows, int *cols) {
    *cols = 80;
    *rows = 24;
    return 0;
}

// 屏幕绘制
static void editorDrawRows(struct abuf *ab, int screenrows) {
    for (int y = 0; y < screenrows; y++) {
        int file_row = y + row_offset;
        if (file_row < total_lines) {
            abAppend(ab, lines[file_row], line_len[file_row]);
        } else if (y == screenrows / 3) {
            char welcome[80];
            int welcomelen = snprintf(welcome, sizeof(welcome),
                "Mini Editor -- 按'q'退出");
            if (welcomelen > 80) welcomelen = 80;
            int padding = (80 - welcomelen) / 2;
            if (padding) {
                abAppend(ab, "~", 1);
                padding--;
            }
            while (padding--) abAppend(ab, " ", 1);
            abAppend(ab, welcome, welcomelen);
        } else {
            abAppend(ab, "~", 1);
        }
        abAppend(ab, "\r\n", 2);
    }
}

// 刷新屏幕
static void editorRefreshScreen(int screenrows) {
    struct abuf ab = ABUF_INIT;
    abAppend(&ab, "\x1b[?25l", 6); // 隐藏光标
    abAppend(&ab, "\x1b[H", 3);    // 光标回到左上角

    editorDrawRows(&ab, screenrows);

    // 光标定位到(cx, cy-row_offset)
    char buf[32];
    snprintf(buf, sizeof(buf), "\x1b[%d;%dH", (cy - row_offset) + 1, cx + 1);
    abAppend(&ab, buf, strlen(buf));

    abAppend(&ab, "\x1b[?25h", 6); // 显示光标

    write(STDOUT_FILENO, ab.b, ab.len);
    abFree(&ab);
}

// 光标移动
static void editorMoveCursor(int key, int screenrows, int screencols) {
    (void)screenrows;
    (void)screencols;
    switch (key) {
        case ARROW_LEFT:
            if (cx > 0) {
                cx--;
            } else if (cy > 0) {
                cy--;
                cx = line_len[cy];
            }
            break;
        case ARROW_RIGHT:
            if (cx < line_len[cy]) {
                cx++;
            } else if (cy < total_lines - 1) {
                cy++;
                cx = 0;
            }
            break;
        case ARROW_UP:
            if (cy > 0) {
                cy--;
                if (cx > line_len[cy]) cx = line_len[cy];
            }
            break;
        case ARROW_DOWN:
            if (cy < total_lines - 1) {
                cy++;
                if (cx > line_len[cy]) cx = line_len[cy];
            }
            break;
    }
}

// 新增：滚动逻辑
static void editorScroll(int screenrows) {
    if (cy < row_offset) {
        row_offset = cy;
    }
    if (cy >= row_offset + screenrows) {
        row_offset = cy - screenrows + 1;
    }
}

// 主编辑循环，接收初始内容
void editorProcessInputLoop(const char *initbuf, int initlen) {
    // 按行初始化多行缓冲区
    total_lines = 0;
    int start = 0;
    for (int i = 0; i <= initlen && total_lines < MAX_ROWS; ++i) {
        if (i == initlen || initbuf[i] == '\n' || initbuf[i] == '\r') {
            int linelen = i - start;
            if (linelen > MAX_COLS - 1) linelen = MAX_COLS - 1;
            memcpy(lines[total_lines], &initbuf[start], linelen);
            line_len[total_lines] = linelen;
            total_lines++;
            start = i + 1;
        }
    }
    if (total_lines == 0) total_lines = 1; // 至少一行
    cx = 0;
    cy = 0;

    int screenrows, screencols;
    getWindowSize(&screenrows, &screencols);

    while (1) {
        editorScroll(screenrows); // 新增：每次刷新前滚动
        editorRefreshScreen(screenrows);

        int c = editorReadKey();
        if (c == 'q') {
            write(STDOUT_FILENO, "\x1b[2J", 4); // 清屏
            write(STDOUT_FILENO, "\x1b[H", 3);  // 光标回左上
            break;
        }
        switch (c) {
            case ARROW_UP:
            case ARROW_DOWN:
            case ARROW_LEFT:
            case ARROW_RIGHT:
                editorMoveCursor(c, screenrows, screencols);
                break;
            case HOME_KEY:
                cx = 0;
                break;
            case END_KEY:
                cx = line_len[cy];
                break;
            case PAGE_UP:
                cy -= screenrows;
                if (cy < 0) cy = 0;
                if (cx > line_len[cy]) cx = line_len[cy];
                row_offset -= screenrows;
                if (row_offset < 0) row_offset = 0;
                break;
            case PAGE_DOWN:
                cy += screenrows;
                if (cy > total_lines - 1) cy = total_lines - 1;
                if (cx > line_len[cy]) cx = line_len[cy];
                row_offset += screenrows;
                if (row_offset > total_lines - screenrows)
                    row_offset = (total_lines - screenrows > 0) ? total_lines - screenrows : 0;
                if (row_offset < 0) row_offset = 0; // 修正
                break;
            case 127: // 退格键
            case '\b':
                if (cx > 0) {
                    memmove(&lines[cy][cx-1], &lines[cy][cx], line_len[cy] - cx);
                    cx--;
                    line_len[cy]--;
                } else if (cy > 0) {
                    int prev_len = line_len[cy-1];
                    if (prev_len + line_len[cy] < MAX_COLS) {
                        memcpy(&lines[cy-1][prev_len], lines[cy], line_len[cy]);
                        line_len[cy-1] += line_len[cy];
                        for (int i = cy; i < total_lines-1; i++) {
                            memcpy(lines[i], lines[i+1], MAX_COLS);
                            line_len[i] = line_len[i+1];
                        }
                        total_lines--;
                        cy--;
                        cx = prev_len;
                    }
                }
                break;
            case '\r': // 回车换行 (有点问题，需改进)
    if (total_lines < MAX_ROWS) {
        int tail_len = line_len[cy] - cx;
        char tail_content[MAX_COLS + 1] = {0};
        if (tail_len > 0) {
            memcpy(tail_content, &lines[cy][cx], tail_len);
        }
        // 截断当前行
        line_len[cy] = cx;

        // 先扩展行数
        total_lines++;
        // 从最后一行往下移，避免覆盖
        for (int i = total_lines - 1; i > cy + 1; i--) {
            memcpy(lines[i], lines[i - 1], MAX_COLS);
            line_len[i] = line_len[i - 1];
        }
        // 新行清零
        memset(lines[cy + 1], 0, MAX_COLS);
        if (tail_len > 0) {
            memcpy(lines[cy + 1], tail_content, tail_len);
            line_len[cy + 1] = tail_len;
        } else {
            line_len[cy + 1] = 0;
        }
        cy++;
        cx = 0;
    }
    break;
            default:
                // 普通字符输入
                if (c >= 32 && c <= 126 && line_len[cy] < MAX_COLS - 1) {
                    memmove(&lines[cy][cx+1], &lines[cy][cx], line_len[cy] - cx);
                    lines[cy][cx] = c;
                    cx++;
                    line_len[cy]++;
                }
                break;
        }
        // 保证光标不越界
        if (cx > line_len[cy]) cx = line_len[cy];
        if (cy > total_lines - 1) cy = total_lines - 1;
    }
}