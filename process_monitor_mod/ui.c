#include <curses.h>
#include "global.h"
#include <dirent.h>
#include <pthread.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/wait.h>
#include <unistd.h>

// 좀비 프로세스 요약 출력
void summarize_zombie_processes()
{   
    // 결과 문자열 형식화
    int pad_width = (col - 50) / 2; // 중앙 정렬을 위한 계산

    int cursor_line = row - 9;
    mvprintw(cursor_line++, pad_width + 3, "===== Zombie Processes Summary =====");
    mvprintw(cursor_line++, pad_width + 7, "Total Zombie Processes: %d\n", zombie_count);

    if (zombie_count > 0)
    {
        attron(COLOR_PAIR(1));
        mvprintw(cursor_line, 1, "Note: Zombie processes cannot be termind directly.\n       Their parent processes must collect them.\n");
        attroff(COLOR_PAIR(1));
        cursor_line += 2;
    }
    mvprintw(cursor_line, pad_width + 3, "=====================================\n");
}
void display_main_menu()
{
    clear();                      // 화면 클리어
    move(1, (col / 2) - 9);       // 커서 이동
    attron(A_BOLD);
    addstr("Process Monitoring"); // 문자열 출력
    attroff(A_BOLD);

    int i = 3;

    move(i++, 1);
    standout();
    addstr("Option list");
    standend();

    move(i++, 1);
    addstr("---------------------------------------------------------------------------------------------");

    // 각 옵션의 이름만 굵은 글씨로 표시
    attron(A_BOLD); // 굵은 글씨 시작
    mvprintw(i, 1, "[CPU]      ");
    attroff(A_BOLD); // 굵은 글씨 끝
    printw("   %s", "View the top 10 processes with the highest CPU usage.");
    i++;

    attron(A_BOLD); // 굵은 글씨 시작
    mvprintw(i, 1, "[MEM]      ");
    attroff(A_BOLD); // 굵은 글씨 끝
    printw("   %s", "View the top 10 processes with the highest Memory usage.");
    i++;

    attron(A_BOLD); // 굵은 글씨 시작
    mvprintw(i, 1, "[SEARCH]   ");
    attroff(A_BOLD); // 굵은 글씨 끝
    printw("   %s", "View information about the desired process.");
    i++;

    attron(A_BOLD); // 굵은 글씨 시작
    mvprintw(i, 1, "[CLEAN]    ");
    attroff(A_BOLD); // 굵은 글씨 끝
    printw("   %s", "Termins unnecessary processes.");
    i++;

    attron(A_BOLD); // 굵은 글씨 시작
    mvprintw(i, 1, "[exit]     ");
    attroff(A_BOLD); // 굵은 글씨 끝
    printw("   %s", "exits the program.");
    i++;

    move(i++, 1);
    move(i++, 1);
    standout();

    if (large_consumed_count > row - 17)
    {
        large_consumed_count = row - 17;
    }
    addstr("List of Running Processes");
    standend();

    move(i++, 1);
    addstr("---------------------------------------------------------------------------------------------");
    move(i++, 1);
    addstr("RANK    PID       Name                      Memory(KB)");

    for (int j = 0; j < large_consumed_count && j < 15; j++) {
        if (j < 3) {
            attron(COLOR_PAIR(1)); // 상위 3개 프로세스는 빨간색으로
        }
        mvprintw(i++, 1, "%-7d %-9d %-25s %-10lu", j + 1, consumed_large[j].pid, consumed_large[j].name, consumed_large[j].memory);
        if (j < 3) {
            attroff(COLOR_PAIR(1)); // 색상 속성 해제
        }
    }

    // 입력 칸 표시
    move(row - 2, 1);
    attron(A_BOLD);
    addstr("Select an option and enter it: ");
    attroff(A_BOLD);
    standout();
    for (int i = 0; i < col - 35; i++)
    {
        addstr(" ");
    }
    standend();

    refresh(); // 화면 갱신
}
