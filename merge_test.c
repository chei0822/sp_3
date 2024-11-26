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

// 전역 변수들
char option[100];
int row, col;
int pipe_fd[2]; // 파이프 생성용

void cpu_top()
{
    close(pipe_fd[0]); // 읽기용 닫기
    while (1)
    {
        // 시스템상의 명령을 실행--> 그 결과를 볼 수 있는 파이프를 연다.
        FILE *fp = popen("ps -eo pid,comm,%cpu --sort=-%cpu | head -n 11", "r");

        if (fp == NULL)
        {
            perror("popen failed");
            exit(1);
        }
        // 임시저장용 버퍼
        char buffer[1024];
        // 버퍼초기화
        memset(buffer, 0, sizeof(buffer));
        fread(buffer, sizeof(char), sizeof(buffer) - 1, fp);
        // 읽어온 데이터를 PIPE의 write_fd로 전달-->부모한테 전달
        write(pipe_fd[1], buffer, strlen(buffer) + 1);

        pclose(fp);

        sleep(1);
    }
}

// 메모리 top 10
void memory_top()
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
        char pid[16], command[128], mem[16];
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

int main()
{
    // SIGINT 신호 설정
    // signal(SIGINT, handle_signal);

    struct winsize wbuf;

    if (ioctl(0, TIOCGWINSZ, &wbuf) != -1)
    {
        row = wbuf.ws_row;
        col = wbuf.ws_col;
    }

    pid_t pid;

    while (1)
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
            endwin(); // ncurses 종료
            break;
        }
        else if (strcmp(option, "CPU") != 0 && strcmp(option, "MEM") != 0 && strcmp(option, "SERCH") != 0 && strcmp(option, "CLEAN") != 0)
        {
            continue;
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
            if (strcmp(option, "SERCH") == 0)
            {
                // 검색한 프로세스 정보를  PIPE로 부모한테 보내는 코드
                close(pipe_fd[0]);

                char pid_input[100];
                move(row - 4, 1);
                addstr("Enter the PID of the process to search: ");
                standout();
                echo();
                move(row - 3, 1);
                getstr(pid_input);
                standend();
                noecho();

                clear();
                move(1, (col / 2) - 9);
                addstr("Process Information\n");
                refresh();

                char status_path[300];
                snprintf(status_path, sizeof(status_path), "/proc/%s/status", pid_input);

                FILE *fp = fopen(status_path, "r");
                if (fp == NULL)
                {
                    perror("Failed to open /proc");
                    exit(1);
                }
                else
                {
                    char line[256];
                    char vm_size[256] = "VmSize: Not found";
                    char vm_rss[256] = "VmRSS: Not found";

                    while (fgets(line, sizeof(line), fp))
                    {
                        if (strncmp(line, "VmSize:", 7) == 0)
                        {
                            strcpy(vm_size, line);
                        }
                        else if (strncmp(line, "VmRSS:", 6) == 0)
                        {
                            strcpy(vm_rss, line);
                        }
                    }
                    fclose(fp);

                    char result[1024];
                    snprintf(result, sizeof(result), "PID: %s\n%s%s", pid_input, vm_size, vm_rss);
                    write(pipe_fd[1], result, strlen(result));
                }
            }
            if (strcmp(option, "CLEAN") == 0)
            {
                // 성민님, 여원님이 작성하신 코드를 바탕으로 불필요한(종료시킨) 프로세스
                // 정보를  PIPE로 부모한테 보내는 코드
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
                    // PIPE의 read_fd에서 읽어서 버퍼에다가 저장
                    read(pipe_fd[0], buffer, sizeof(buffer));

                    // 화면 초기화
                    clear();
                    mvprintw(0, 0, "Top 10 CPU Usage Processes:");
                    mvprintw(1, 0, "%s", buffer);
                    refresh(); // 실제로 표시

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
            if (strcmp(option, "SERCH") == 0)
            {
                memset(buffer, 0, sizeof(buffer));
                int n_read = read(pipe_fd[0], buffer, sizeof(buffer) - 1);

                if (n_read > 0)
                {
                    buffer[n_read] = '\0';
                    clear();
                    move(3, 1);
                    addstr("Search Results:");
                    move(5, 1);
                    addstr(buffer);
                    refresh();

                    move(row - 2, 1);
                    addstr("Do you want to kill this process? (y/n):");
                    refresh();

                    int ch = getch();
                    if (ch == 'y' || ch == 'Y')
                    {
                        char pid_to_kill[100];
                        sscanf(buffer, "PID: %s", pid_to_kill); // PID 추출
                        pid_t target_pid = atoi(pid_to_kill);

                        if (kill(target_pid, SIGTERM) == 0)
                        {
                            move(row - 2, 1);
                            addstr("Process terminated successfully.");
                        }
                        else
                        {
                            move(row - 2, 1);
                            addstr("Failed to terminate the process. Insufficient permissions?");
                        }
                    }
                    else
                    {
                        mvprintw(row - 1, 1, "Process termination canceled.");
                    }
                    refresh();
                }
                else
                {
                    move(row - 2, 1);
                    addstr("Process termination canceled.");
                }
                move(row, 1);
                addstr("Press 'q' to return to the main menu.");
                refresh();
                while (1)
                {
                    int ch = getch();
                    // 메인메뉴로빠져나가기
                    if (ch == 'q' || ch == 'Q')
                    {
                        kill(pid, SIGTERM);
                        waitpid(pid, NULL, 0);
                        break;
                    }
                }
            }
            if (strcmp(option, "CLEAN") == 0)
            {
            }

            // 메인 화면으로 돌아가기 위해 curses 상태 갱신
            close(pipe_fd[0]); // 읽기용 닫기, 완전히 닫음
            nocbreak();
            nodelay(stdscr, FALSE);

            endwin(); // ncurses 종료
        }
    }

    printf("Exiting safely...\n\r");
    return 0;
}
