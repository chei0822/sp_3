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
    int cursor_line = row / 2;
    mvprintw(cursor_line++, 1, "===== Zombie Processes Summary =====");
    mvprintw(cursor_line++, 1, "Total Zombie Processes: %d\n", zombie_count);

    if (zombie_count > 0)
    {
        mvprintw(cursor_line, 1, "Note: Zombie processes cannot be terminated directly.\n       Their parent processes must collect them.\n");
        cursor_line += 2;
    }
    mvprintw(cursor_line, 1, "=====================================\n");
}
void display_main_menu()
{
    clear();                      // 화면 클리어
    move(1, (col / 2) - 9);       // 커서 이동
    addstr("Process Monitoring"); // 문자열 출력

    int i = 3;

    move(i++, 1);
    standout();
    addstr("Option list");
    standend();

    move(i++, 1);                                                             // 커서 이동
    addstr("CPU - View the top 10 processes with the highest CPU usage.");    // 문자열
                                                                              // 출력
    move(i++, 1);                                                             // 커서 이동
    addstr("MEM - View the top 10 processes with the highest Memory usage."); // 문자열
                                                                              // 출력
    move(i++, 1);                                                             // 커서 이동
    addstr("SEARCH - View information about the desired process.");           // 문자열 출력
    move(i++, 1);                                                             // 커서 이동
    addstr("CLEAN - Terminates unnecessary processes.");                      // 문자열 출력
    move(i++, 1);                                                             // 커서 이동
    addstr("exit - exits the program.");                                      // 문자열 출력
                                                                              // diplay_main_menu 에 추가
    move(i++, 1);
    move(i++, 1);
    standout();
    if (large_consumed_count > row - 17)
    {
        large_consumed_count = row - 17;
    }
    addstr("List of Running Processes:");
    standend();

    move(i++, 1);
    addstr("---------------------------------------------------------------");
    move(i++, 1);
    addstr("  PID      Name                 Memory (KB)");

    for (int j = 0; j < large_consumed_count && j < 15; j++)
    {
        mvprintw(i++, 1, "%d: %-8d %-20s %-10lu", j + 1, consumed_large[j].pid, consumed_large[j].name, consumed_large[j].memory);
    }

    // 입력 칸 표시
    move(row - 2, 1);
    addstr("option:");
    standout();
    for (int i = 0; i < col - 10; i++)
    {
        addstr(" ");
    }
    standend();

    refresh(); // 화면 갱신
}
