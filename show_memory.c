#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>
#include <signal.h>
#include <dirent.h>
#include <curses.h>
#include <sys/ioctl.h>
#include <sys/wait.h> // 추가

// 전역 변수들
char option[100];
int row, col;

int main()
{
    struct winsize wbuf;

    if (ioctl(0, TIOCGWINSZ, &wbuf) != -1)
    {
        row = wbuf.ws_row;
        col = wbuf.ws_col;
    }

    pid_t pid;
    int pipe_fd[2]; // 파이프 생성용 // 추가

    // 파이프 생성 // 추가
    if(pipe(pipe_fd) == -1){
        perror("pipe failed");

        exit(1);
    }

    initscr(); // ncurses 초기화
    echo();    // 입력 문자를 화면에 출력
    curs_set(FALSE);

    while (1)
    {
        // clear(); // 화면 클리어
        move(1, (col / 2) - 9);       // 커서 이동
        addstr("Process Monitoring"); // 문자열 출력

        int i = 3;

        move(i++, 1);
        standout();
        addstr("Option list");
        standend();

        move(i++, 1);                                                             // 커서 이동
        addstr("CPU - View the top 10 processes with the highest CPU usage.");    // 문자열 출력
        move(i++, 1);                                                             // 커서 이동
        addstr("MEM - View the top 10 processes with the highest Memory usage."); // 문자열 출력
        move(i++, 1);                                                             // 커서 이동
        addstr("SEARCH - View information about the desired process.");           // 문자열 출력
        move(i++, 1);                                                             // 커서 이동
        addstr("CLEAN - Terminates unnecessary processes.");                      // 문자열 출력
        move(i++, 1);                                                             // 커서 이동
        addstr("exit - exits the program.");                                      // 문자열 출력

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

        move(row - 2, 9);
        standout();
        getstr(option);
        standend();

        if (strcmp(option, "exit") == 0)
        {
            break;
        }
        else if (strcmp(option, "CPU") != 0 && strcmp(option, "MEM") != 0 && strcmp(option, "SERCH") != 0 && strcmp(option, "CLEAN") != 0)
        {
            move(6, 5);
            addstr("check option.");
            refresh(); // 화면 갱신
        }

        pid = fork();

        if (pid == 0)
        {
            if (strcmp(option, "CPU") == 0)
            {
                // 1초 주기로 CPU 사용량 높은 순위 10개 목록을 PIPE로 부모한테 보내는 코드
            }

            if (strcmp(option, "MEM") == 0)
            {
                // 1초 주기로 memory 사용량 높은 순위 10개 목록을 PIPE로 부모한테 보내는 코드
                close(pipe_fd[0]); // 읽기용 닫기

                while (1)
                {
                    FILE *fp = popen("ps -eo pid,comm,%mem --sort=-%mem | awk 'NR>1'", "r");

                    if (fp == NULL)
                    {
                        perror("popen failed");
                        exit(1);
                    }

                    char buffer[1024] = {0};
                    char result[1024] = {0};
                    char pid[16], command[256], mem[16];
                    int rank = 1;

                    strcat(result, "Rank  COMMAND          PID      Memory(Kb)\n");

                    while (fgets(buffer, sizeof(buffer), fp) && rank <= 10)
                    {
                        sscanf(buffer, "%s %s %s", pid, command, mem);

                        // %MEM(Kb) 변환
                        float mem_percent = atof(mem);
                        long mem_kb = (long)((mem_percent / 100.0) * sysconf(_SC_PHYS_PAGES) * sysconf(_SC_PAGE_SIZE) / 1024);

                        char line[256];
                        sprintf(line, "%-5d %-16s %-8s %-10ld\n", rank++, command, pid, mem_kb);
                        strcat(result, line);
                    }

                    pclose(fp);
                    write(pipe_fd[1], result, strlen(result) + 1);
                    sleep(1);
                }
            }

            if (strcmp(option, "SEARCH") == 0)
            {
                // 검색한 프로세스 정보를  PIPE로 부모한테 보내는 코드
            }

            if (strcmp(option, "CLEAN") == 0)
            {
                // 성민님, 여원님이 작성하신 코드를 바탕으로 불필요한(종료시킨) 프로세스 정보를  PIPE로 부모한테 보내는 코드
            }
        }
        else
        {
            // 부모 프로세스
            // 각 option에 맞는 화면 curses 구현 - while(1)
            // "q"를 입력하면 자식 프로세스를 종료시키고 break

            if (strcmp(option, "CPU") == 0)
            {
            }
            if (strcmp(option, "MEM") == 0)
            {
                close(pipe_fd[1]); // 쓰기용 닫기

                initscr();
                cbreak();
                noecho();
                nodelay(stdscr, TRUE);

                char buffer[1024];

                while (1)
                {
                    memset(buffer, 0, sizeof(buffer));
                    read(pipe_fd[0], buffer, sizeof(buffer));

                    clear();
                    mvprintw(0, 0, "Top 10 Memory Usage Processes:");
                    mvprintw(1, 0, "%s", buffer);
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
            }
            if (strcmp(option, "CLEAN") == 0)
            {
            }
        }
    }

    endwin(); // ncurses 종료

    printf("Exiting safely...\n");
    return 0;
}