#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include "editor.h"
#include "rawmode.h"
#include <time.h>

#define ABUF_INIT {NULL, 0}
#define MAX_ROWS 100
#define MAX_COLS 256
#define TAB_STOP 4 // 或8，根据需要修改

enum editorKey {
    ARROW_LEFT = 1000,
    ARROW_RIGHT,
    ARROW_UP,
    ARROW_DOWN,
    HOME_KEY,
    END_KEY,
    PAGE_UP,
    PAGE_DOWN,
    DEL_KEY // 新增
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

// 删除的内容：
// void die(const char *s) {
//     perror(s);
//     exit(1);
// }

// 只保留声明（如果需要）：
void die(const char *s);

// 多行文本缓冲区
static char lines[MAX_ROWS][MAX_COLS] = {{0}};
static int line_len[MAX_ROWS] = {0};
static int total_lines = 1;

// 光标位置
static int cx = 0;
static int cy = 0;

// 新增：屏幕滚动偏移
static int row_offset = 0;

// 新增：脏标志
static int dirty = 0;
static char current_filename[256] = {0}; // 当前文件名
static int quit_times = 3; // 退出确认次数

// 状态栏消息
static char statusmsg[80] = {0};
static time_t statusmsg_time = 0;

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
                        case '3': return DEL_KEY; // 修改为DEL_KEY
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

// 获取终端窗口大小
static int getWindowSize(int *rows, int *cols) {
    *cols = 80;
    *rows = 24;
    return 0;
}

// 渲染带制表符的行
static void renderLineWithTab(const char *src, int len, char *dest, int maxlen) {
    int j = 0;
    for (int i = 0; i < len && j < maxlen - 1; i++) {
        if (src[i] == '\t') {
            int spaces = TAB_STOP - (j % TAB_STOP);
            while (spaces-- && j < maxlen - 1) dest[j++] = ' ';
        } else {
            dest[j++] = src[i];
        }
    }
    dest[j] = '\0';
}

// 状态栏渲染，增加脏标志
static void drawStatusBar(struct abuf *ab, const char *filename, int total_lines, int cy) {
    char status[80];
    snprintf(status, sizeof(status), "%.20s%s | %d lines | Ln %d",
        (filename && filename[0]) ? filename : "No Name",
        dirty ? " (modified)" : "",
        total_lines, cy + 1);
    abAppend(ab, "\x1b[7m", 4); // 反色
    abAppend(ab, status, strlen(status));
    int pad = 80 - (int)strlen(status);
    while (pad-- > 0) abAppend(ab, " ", 1);
    abAppend(ab, "\x1b[m", 3);
    abAppend(ab, "\r\n", 2);
}

// 修改 editorDrawRows，使用 renderLineWithTab 渲染每一行
static void editorDrawRows(struct abuf *ab, int screenrows) {
    char renderbuf[MAX_COLS * TAB_STOP + 1];
    for (int y = 0; y < screenrows - 2; y++) { // 留2行给状态栏和消息栏
        int file_row = y + row_offset;
        if (file_row < total_lines) {
            renderLineWithTab(lines[file_row], line_len[file_row], renderbuf, sizeof(renderbuf));
            int len = strlen(renderbuf);
            abAppend(ab, renderbuf, len);
            // 补足空格到80列，防止残影
            for (int i = len; i < 80; i++) abAppend(ab, " ", 1);
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
            for (int i = 1; i < 80; i++) abAppend(ab, " ", 1);
        }
        abAppend(ab, "\r\n", 2);
    }
}

// 消息栏（预留一行，后续可用于搜索等）
static void drawMessageBar(struct abuf *ab) {
    abAppend(ab, "\x1b[K", 3); // 清空
    if (statusmsg[0] && time(NULL) - statusmsg_time < 5) {
        abAppend(ab, statusmsg, strlen(statusmsg));
    }
    abAppend(ab, "\r\n", 2);
}

// 修改 editorRefreshScreen，增加 filename 参数
static void editorRefreshScreen(int screenrows, const char *filename) {
    struct abuf ab = ABUF_INIT;
    abAppend(&ab, "\x1b[?25l", 6); // 隐藏光标
    abAppend(&ab, "\x1b[H", 3);    // 光标回到左上角

    editorDrawRows(&ab, screenrows);

    // 状态栏
    drawStatusBar(&ab, filename, total_lines, cy);
    drawMessageBar(&ab);

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
void editorProcessInputLoop(const char *initbuf, int initlen, const char *filename) {
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

    // 文件名赋值
    if (filename && filename[0]) strncpy(current_filename, filename, sizeof(current_filename)-1);

    int screenrows, screencols;
    getWindowSize(&screenrows, &screencols);

    while (1) {
        editorScroll(screenrows);
        editorRefreshScreen(screenrows, current_filename);

        int c = editorReadKey();
        if (c == 'q' || c == 17) { // 'q' 或 Ctrl-Q
            if (dirty && quit_times > 0) {
                snprintf(statusmsg, sizeof(statusmsg),
                    "有未保存更改，连续按Ctrl-Q %d次退出", quit_times);
                statusmsg_time = time(NULL);
                quit_times--;
                continue;
            }
            write(STDOUT_FILENO, "\x1b[2J", 4);
            write(STDOUT_FILENO, "\x1b[H", 3);
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
                if (row_offset < 0) row_offset = 0;
                break;
            case 127: // 退格键
            case '\b':
                if (cx > 0) {
                    memmove(&lines[cy][cx-1], &lines[cy][cx], line_len[cy] - cx + 1);
                    cx--;
                    line_len[cy]--;
                    lines[cy][line_len[cy]] = '\0';
                    dirty = 1;
                } else if (cy > 0) {
                    int prev_len = line_len[cy-1];
                    if (prev_len + line_len[cy] < MAX_COLS) {
                        memcpy(&lines[cy-1][prev_len], lines[cy], line_len[cy] + 1);
                        line_len[cy-1] += line_len[cy];
                        lines[cy-1][line_len[cy-1]] = '\0';
                        for (int i = cy; i < total_lines-1; i++) {
                            memcpy(lines[i], lines[i+1], MAX_COLS);
                            line_len[i] = line_len[i+1];
                        }
                        memset(lines[total_lines-1], 0, MAX_COLS);
                        line_len[total_lines-1] = 0;
                        total_lines--;
                        cy--;
                        cx = prev_len;
                        dirty = 1;
                    }
                }
                break;

            case DEL_KEY: // Delete键
                if (cx < line_len[cy]) {
                    memmove(&lines[cy][cx], &lines[cy][cx+1], line_len[cy] - cx);
                    line_len[cy]--;
                    lines[cy][line_len[cy]] = '\0';
                    dirty = 1;
                } else if (cy < total_lines - 1) {
                    if (line_len[cy] + line_len[cy+1] < MAX_COLS) {
                        memcpy(&lines[cy][line_len[cy]], lines[cy+1], line_len[cy+1] + 1);
                        line_len[cy] += line_len[cy+1];
                        for (int i = cy+1; i < total_lines-1; i++) {
                            memcpy(lines[i], lines[i+1], MAX_COLS);
                            line_len[i] = line_len[i+1];
                        }
                        memset(lines[total_lines-1], 0, MAX_COLS);
                        line_len[total_lines-1] = 0;
                        total_lines--;
                        dirty = 1;
                    }
                }
                break;

            case '\r': // 回车换行
                if (total_lines < MAX_ROWS) {
                    int tail_len = line_len[cy] - cx;
                    char tail_content[MAX_COLS] = {0};
                    if (tail_len > 0) {
                        memcpy(tail_content, &lines[cy][cx], tail_len + 1);
                    }
                    memset(&lines[cy][cx], 0, MAX_COLS - cx);
                    line_len[cy] = cx;
                    lines[cy][cx] = '\0';

                    total_lines++;
                    for (int i = total_lines - 1; i > cy + 1; i--) {
                        memcpy(lines[i], lines[i - 1], MAX_COLS);
                        line_len[i] = line_len[i - 1];
                    }
                    memset(lines[cy + 1], 0, MAX_COLS);
                    if (tail_len > 0) {
                        memcpy(lines[cy + 1], tail_content, tail_len + 1);
                        line_len[cy + 1] = tail_len;
                        lines[cy + 1][tail_len] = '\0';
                    } else {
                        line_len[cy + 1] = 0;
                        lines[cy + 1][0] = '\0';
                    }
                    cy++;
                    cx = 0;
                    dirty = 1;
                }
                break;

            case 19: // Ctrl-S 保存
                if (current_filename[0] == '\0') {
                    // 退出原始模式
                    disableRawMode();
                    printf("\r\n请输入文件名保存: ");
                    fflush(stdout);
                    if (fgets(current_filename, sizeof(current_filename), stdin) == NULL) {
                        enableRawMode();
                        break;
                    }
                    size_t len = strlen(current_filename);
                    if (len > 0 && current_filename[len-1] == '\n')
                        current_filename[len-1] = '\0';
                    enableRawMode();

                    // 清屏并回到左上角，去除输入提示
                    write(STDOUT_FILENO, "\x1b[2J", 4);
                    write(STDOUT_FILENO, "\x1b[H", 3);
                }
                if (current_filename[0] == '\0') break; // 用户没输入文件名
                FILE *fp = fopen(current_filename, "w");
                if (fp) {
                    for (int i = 0; i < total_lines; i++) {
                        fwrite(lines[i], 1, line_len[i], fp);
                        fputc('\n', fp);
                    }
                    fclose(fp);
                    dirty = 0;
                    // 可用消息栏提示“保存成功”
                } else {
                    // 可用消息栏提示“保存失败”
                }
                break;

            default:
                if (c >= 32 && c <= 126 && line_len[cy] < MAX_COLS - 1) {
                    memmove(&lines[cy][cx+1], &lines[cy][cx], line_len[cy] - cx + 1);
                    lines[cy][cx] = c;
                    cx++;
                    line_len[cy]++;
                    lines[cy][line_len[cy]] = '\0';
                    dirty = 1;
                }
                break;
        }

        // 保证光标和行数不越界
        if (cx > line_len[cy]) cx = line_len[cy];
        if (cy > total_lines - 1) cy = total_lines - 1;
        if (cy < 0) cy = 0;
        if (cx < 0) cx = 0;
    }
}