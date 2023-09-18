/******************************************************************************
 * Copyright (c) China Unicom Technologies Co., Ltd. 2023. All rights reserved.
 * iSulad licensed under the Mulan PSL v2.
 * You can use this software according to the terms and conditions of the Mulan PSL v2.
 * You may obtain a copy of Mulan PSL v2 at:
 *     http://license.coscl.org.cn/MulanPSL2
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY OR FIT FOR A PARTICULAR
 * PURPOSE.
 * See the Mulan PSL v2 for more details.
 * Author: Chenwei
 * Create: 2023-08-25
 * Description: print progress
 ******************************************************************************/

#include "progress.h"
#include <unistd.h>
#include <sys/ioctl.h>
#include <termios.h>
#include <stdio.h>

#define GB (1024 * 1024 * 1024)
#define MB (1024 * 1024)
#define KB 1024

void move_to_row(int row)
{
    printf("\033[%d;1H", row);
    fflush(stdout);
}

void move_cursor_up(int rows)
{
    printf("\033[%dA", rows);  // ANSI escape code to move cursor up 'rows' rows
}

void clear_row(int row)
{
    move_to_row(row);
    printf("\033[2K");
    fflush(stdout);
}

void clear_lines_below()
{
    printf("\x1b[J");  // ANSI escape code to clear from cursor to end of screen
    fflush(stdout);
}

int get_current_row()
{
    struct termios term;
    if (tcgetattr(STDOUT_FILENO, &term) == -1) {
        perror("tcgetattr");
        return -1;
    }
    return term.c_cc[VERASE];
}

int get_terminal_width()
{
    struct winsize ws;
    if (ioctl(STDOUT_FILENO, TIOCGWINSZ, &ws) == -1) {
        perror("ioctl");
        return -1; // Error
    }
    return ws.ws_col;
}

static void get_printed_value(int64_t value, char *printed)
{
    float float_value = 0.0;

    if ((float)value / GB > 1) {
        float_value = (float)value / GB;
        sprintf(printed, "%.2fGB", float_value);
    } else if ((float)value / MB > 1) {
        float_value = (float)value / MB;
        sprintf(printed, "%.2fMB", float_value);
    } else if ((float)value / KB > 1) {
        float_value = (float)value / KB;
        sprintf(printed, "%.2fKB", float_value);
    } else {
        sprintf(printed, "%ldB", value);
    }
}

void display_progress_bar(image_progress_progresses_element *progress_item, int width, bool if_show_all)
{
    int i = 0;
    float progress = 0.0;
    int filled_width = 0;
    char total[16] = {0};
    char current[16] = {0};
    int empty_width = 0;

    if (progress_item->total != 0) {
        progress = (float)progress_item->current / (float)progress_item->total;
    }
    filled_width = (int)(progress * width);
    empty_width = width - filled_width;
    get_printed_value(progress_item->total, total);
    get_printed_value(progress_item->current, current);

    if (if_show_all) {
        printf("%s: [", progress_item->id);

        // Print filled characters
        for (i = 0; i < filled_width; i++) {
            printf("=");
        }
        printf(">");
        // Print empty characters
        for (i = 0; i < empty_width; i++) {
            printf(" ");
        }

        printf("] %s/%s", current, total);
    } else {
        printf("%s:  %s/%s", progress_item->id, current, total);
    }
    fflush(stdout);
}

void display_multiple_progress_bars(image_progress *progresses, int width)
{
    size_t i = 0;
    static int len = 0;

    if (len != 0) {
        move_cursor_up(len);
    }
    clear_lines_below();
    len = (int)progresses->progresses_len;
    int terminal_width = get_terminal_width();
    bool if_show_all = true;
    if (terminal_width < 110) {
        if_show_all = false;
    }
    for (i = 0; i < len; i++) {
        display_progress_bar(progresses->progresses[i], width, if_show_all);
        printf("\n");
    }
}

void show_processes(image_progress *progresses)
{
    int width = 50;  // Width of the progress bars

    display_multiple_progress_bars(progresses, width);

    return;
}
