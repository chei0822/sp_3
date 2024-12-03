#include <curses.h>
#include <dirent.h>
#include <pthread.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/wait.h>
#include <unistd.h>
#include "utils.h"
#include "ui.h"
#include "process_handler.h"
#include "global.h"

char option[100];
char pid_input[100];
int row, col;
int pipe_fd[2];
int terminate = 0; // 프로그램 종료 플래그

ProcessInfo consumed_large[1024];
ProcessInfo low_priority_processes[1024];
int large_consumed_count = 0;
int low_priority_count = 0;

ProcessInfo inactive_processes[1024];
int inactive_count = 0;
ProcessInfo zombie_processes[1024];
int zombie_count = 0;

const char *SYSTEM_PROCESSES[] = {
    "init", "systemd", "kthreadd", "kworker", "sshd", "dbus-daemon", "cron", "journald", "rsyslogd"
};
const int SYSTEM_PROCESS_COUNT = 9;



int main()
{
    // SIGINT 신호 설정
    signal(SIGINT, handle_signal);

    struct winsize wbuf;

    if (ioctl(0, TIOCGWINSZ, &wbuf) != -1)
    {
        row = wbuf.ws_row;
        col = wbuf.ws_col;
    }

    pid_t pid;

    while (!terminate)
    {
        initscr(); // ncurses 초기화
        echo();    // 입력 문자를 화면에 출력
        curs_set(FALSE);
        // 파이프 생성 // 추가
        if (pipe(pipe_fd) == -1)
        {
            perror("pipe failed");
            exit(1);
        }

        get_process_info();
        sort_large_consumed_processes();
        display_main_menu();

        move(row - 2, 9);
        standout();
        getstr(option);
        standend();

        while (strcmp(option, "CPU") != 0 && strcmp(option, "MEM") != 0 && strcmp(option, "SEARCH") != 0 && strcmp(option, "CLEAN") != 0 && strcmp(option, "ALARM") != 0)
        {
            if (strcmp(option, "exit") == 0)
            {
                endwin(); // ncurses 종료
                printf("Exiting safely...\n\r");
                return 0;
            }
            move(row - 1, 1);
            addstr("check option");

            standout();
            move(row - 2, 9);
            for (int i = 0; i < col - 10; i++)
            {
                addstr(" ");
            }
            move(row - 2, 9);
            getstr(option);
            standend();
        }

        if (strcmp(option, "SEARCH") == 0)
        {
            move(row - 4, 1);
            addstr("Enter the PID of the process to search: ");
            standout();
            echo();
            move(row - 3, 1);
            getstr(pid_input);
            standend();
            noecho();
        }

        if (strcmp(option, "CLEAN") == 0)
        {
            clear();
            // 프로세스 정보 수집
            get_process_info();

            // 우선순위가 낮은 프로세스를 정렬
            sort_low_priority_processes();

            // 우선순위가 낮은 프로세스를 종료
            terminate_low_priority_processes();

            // 좀비 프로세스 정보 요약
            summarize_zombie_processes();

            int ch = getch();
            if (ch == 'q')
            {
                continue;
            }
        }

        pid = fork();

        if (pid == 0)
        {
            if (strcmp(option, "CPU") == 0)
            {
                // 1초 주기로 CPU 사용량 높은 순위 10개 목록을 PIPE로 부모한테 보내는 코드
                cpu_top();
            }
            if (strcmp(option, "MEM") == 0)
            {
                // 1초 주기로 memory 사용량 높은 순위 10개 목록을 PIPE로 부모한테 보내는 코드
                memory_top();
            }
            if (strcmp(option, "SEARCH") == 0)
            {
                // 검색한 프로세스 정보를  PIPE로 부모한테 보내는 코드
                serch_process();
            }
            if (strcmp(option, "ALARM") == 0)
            {
                // 검색한 프로세스 정보를  PIPE로 부모한테 보내는 코드
                alarm_process();
            }
        }
        else
        {
            // 부모 프로세스
            // 각 option에 맞는 화면 curses 구현 - while(1)
            // "q"를 입력하면 자식 프로세스를 종료시키고 break

            close(pipe_fd[1]); // 쓰기용 닫기
            nodelay(stdscr, TRUE);
            char buffer[1024];

            if (strcmp(option, "CPU") == 0)
            {
                while (1)
                {
                    memset(buffer, 0, sizeof(buffer));
                    read(pipe_fd[0], buffer, sizeof(buffer));

                    // 화면 초기화
                    clear();

                    // 화면 중앙 정렬
                    int row, col;
                    getmaxyx(stdscr, row, col);
                    int start_row = (row / 2) - 5;  // 시작 행
                    int start_col = (col - 50) / 2; // 시작 열

                    // 제목 출력
                    mvprintw(start_row - 2, start_col, "Top 10 CPU Usage Processes:");

                    // 데이터 출력
                    char *line = strtok(buffer, "\n");
                    int line_count = 0;

                    while (line != NULL)
                    {
                        mvprintw(start_row + line_count, start_col, "%s", line);
                        line = strtok(NULL, "\n");
                        line_count++;
                    }

                    refresh(); // 화면 갱신

                    // 종료 입력
                    int ch = getch();
                    if (ch == 'q')
                    {
                        break;
                    }
                }
            }

            if (strcmp(option, "MEM") == 0)
            {
                while (1)
                {
                    memset(buffer, 0, sizeof(buffer));
                    read(pipe_fd[0], buffer, sizeof(buffer));

                    clear();

                    // 화면 중앙에 내용 출력
                    int start_row = (row / 2) - 5;  // 중앙 정렬 시작 행
                    int start_col = (col - 50) / 2; // 중앙 정렬 시작 열

                    mvprintw(start_row - 2, start_col, "Top 10 Memory Usage Processes:");

                    // 행렬 데이터를 각 줄로 나누어 출력
                    char *line = strtok(buffer, "\n");
                    int line_count = 0;

                    while (line != NULL)
                    {
                        mvprintw(start_row + line_count, start_col, "%s", line); // 각 줄 출력
                        line = strtok(NULL, "\n");
                        line_count++;
                    }

                    refresh();

                    int ch = getch();
                    if (ch == 'q')
                    {
                        kill(pid, SIGTERM);
                        waitpid(pid, NULL, 0);
                        break;
                    }
                }
            }
            if (strcmp(option, "SEARCH") == 0)
            {
                while (1)
                {
                    memset(buffer, 0, sizeof(buffer));
                    int n_read = read(pipe_fd[0], buffer, sizeof(buffer));

                    clear();
                    move(1, (col / 2) - 9);
                    addstr("Process Information\n");
                    mvprintw(3, 1, "Search Results:");
                    mvprintw(5, 1, "%s", buffer);

                    if (n_read == 0)
                    {
                        mvprintw(5, 1, "Process not found");

                        break;
                    }
                    mvprintw(row - 4, 1, "Do you want to kill this process? (y/n):");
                    refresh();
                    int ch = getch();
                    if (ch == 'y' || ch == 'Y')
                    {
                        char pid_to_kill[100];
                        sscanf(buffer, "PID: %s", pid_to_kill); // PID 추출
                        pid_t target_pid = atoi(pid_to_kill);

                        if (kill(target_pid, SIGTERM) == 0)
                        {
                            move(row - 3, 1);
                            addstr("Process terminated successfully.");
                            break;
                        }
                        else
                        {
                            move(row - 3, 1);
                            addstr("Failed to terminate the process. Insufficient permissions?");
                            break;
                        }
                    }
                    else if (ch == 'n' || ch == 'N')
                    {
                        mvprintw(row - 3, 1, "Process termination canceled.");
                        break;
                    }
                }

                mvprintw(row - 2, 1, "Press 'q' to return to the main menu.");
                refresh();

                while (1)
                {
                    int ch = getch();
                    // 메인메뉴로빠져나가기
                    if (ch == 'q' || ch == 'Q')
                    {
                        // kill(pid,SIGTERM);
                        clear();
                        break;
                    }
                }
            }
            if (strcmp(option, "ALARM") == 0)
            {
                while (1)
                {
                    memset(buffer, 0, sizeof(buffer));
                    read(pipe_fd[0], buffer, sizeof(buffer));

                    clear();

                    // 화면 중앙에 내용 출력
                    int start_row = (row / 2) - 5;  // 중앙 정렬 시작 행
                    int start_col = (col - 50) / 2; // 중앙 정렬 시작 열

                    mvprintw(start_row - 2, start_col, "List of processes using more than 30 %% of memory");

                    // 행렬 데이터를 각 줄로 나누어 출력
                    char *line = strtok(buffer, "\n");
                    int line_count = 0;

                    while (line != NULL)
                    {
                        mvprintw(start_row + line_count, start_col, "%s", line); // 각 줄 출력
                        line = strtok(NULL, "\n");
                        line_count++;
                    }

                    refresh();

                    int ch = getch();
                    if (ch == 'q')
                    {
                        kill(pid, SIGTERM);
                        waitpid(pid, NULL, 0);
                        break;
                    }
                }
            }

            // 메인 화면으로 돌아가기 위해 curses 상태 갱신
            close(pipe_fd[0]); // 읽기용 닫기, 완전히 닫음
            nocbreak();
            nodelay(stdscr, FALSE);
            kill(pid, SIGTERM);
            waitpid(pid, NULL, 0);

            endwin(); // ncurses 종료
        }
    }

    printf("Exiting safely...\n\r");
    return 0;
}
