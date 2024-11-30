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
char pid_input[100];
int row, col;
int pipe_fd[2];    // 파이프 생성용
int terminate = 0; // 프로그램 종료 플래그

typedef struct
{
    int pid;
    unsigned long memory;
    char command[256];
    int priority;
    char state;
    char name[256];
} ProcessInfo;

ProcessInfo consumed_large[1024];
ProcessInfo low_priority_processes[1024];
int large_consumed_count = 0;
int low_priority_count = 0;

// 비활성화된 프로세스 저장할 배열
ProcessInfo inactive_processes[1024];
int inactive_count = 0;
// 좀비프로세스 저장할 배열
ProcessInfo zombie_processes[1024];
int zombie_count = 0;

// 시스템 프로세스 목록
const char *SYSTEM_PROCESSES[] = {"init", "systemd", "kthreadd", "kworker", "sshd", "dbus-daemon", "cron", "journald", "rsyslogd"};
const int SYSTEM_PROCESS_COUNT = 9;

// SIGINT 신호 핸들러
void handle_signal(int signal) { terminate = 1; }

// 시스템 프로세스인지 확인하는 함수
int is_system_process(const char *name, int pid)
{
    for (int i = 0; i < SYSTEM_PROCESS_COUNT; i++)
    {
        if (strcmp(name, SYSTEM_PROCESSES[i]) == 0)
        {
            return 1;
        }
    }
    if (pid >= 1 && pid <= 100)
    {
        return 1;
    }
    // 쉘 프로세스 예외 처리
    const char *shells[] = {"bash", "sh", "zsh", "ksh", "fish", "tcsh"};
    for (int i = 0; i < sizeof(shells) / sizeof(shells[0]); i++)
    {
        if (strcmp(name, shells[i]) == 0)
        {
            return 1; // 쉘 프로세스는 종료X        }
        }
        return 0;
    }
}

void get_process_info()
{
    DIR *dir = opendir("/proc");
    struct dirent *entry;
    large_consumed_count = 0;

    if (dir == NULL)
    {
        perror("Could not open /proc directory");
        return;
    }

    low_priority_count = 0;
    // 비활성화 프로세스&좀비프로세스 count
    inactive_count = 0;
    zombie_count = 0;

    while ((entry = readdir(dir)) != NULL)
    {
        if (entry->d_type == DT_DIR && atoi(entry->d_name) > 0)
        { // 프로세스 디렉터리만 선택
            char status_path[300], stat_path[300], line[300];
            snprintf(status_path, sizeof(status_path), "/proc/%s/status", entry->d_name);
            snprintf(stat_path, sizeof(stat_path), "/proc/%s/stat", entry->d_name);

            ProcessInfo pinfo = {0};
            pinfo.pid = atoi(entry->d_name);
            pinfo.memory = 0;

            // 메모리 정보 읽기
            FILE *status_file = fopen(status_path, "r");
            if (status_file != NULL)
            {
                while (fgets(line, sizeof(line), status_file))
                {
                    if (strncmp(line, "Name:", 5) == 0)
                    {
                        sscanf(line, "Name: %s", pinfo.name);
                    }
                    if (strncmp(line, "VmRSS:", 6) == 0)
                    {
                        sscanf(line, "VmRSS: %lu kB", &pinfo.memory);
                    }
                    if (strncmp(line, "State:", 6) == 0)
                    {
                        sscanf(line, "State: %c", &pinfo.state);
                    }
                }
                fclose(status_file);
            }

            // Priority 정보 읽기
            FILE *stat_file = fopen(stat_path, "r");
            if (stat_file != NULL)
            {
                if (fgets(line, sizeof(line), stat_file))
                {
                    int pid;
                    char comm[256];
                    char state;
                    int priority;

                    // 필드 순서에 맞게 정확히 읽음
                    sscanf(line,
                           "%d %255s %c "
                           "%*d %*d %*d %*d %*d %*u %*u %*u %*u %*u %*u %*u %*d %*d %*d "
                           "%*d %*d %d",
                           &pid, comm, &state, &priority);

                    pinfo.pid = pid;
                    strncpy(pinfo.command, comm, sizeof(pinfo.command));
                    pinfo.priority = priority;
                }
                fclose(stat_file);
            }
            else
            {
                perror("Failed to open stat file");
            }

            // 조건을 만족하는 프로세스만 추가
            if (pinfo.pid > 1)
            {
                consumed_large[large_consumed_count++] = pinfo;
            }

            // 시스템 프로세스인지 아닌지 확인
            if (is_system_process(pinfo.name, pinfo.pid))
            {
                continue;
            }

            if (pinfo.priority >= 0)
            {
                low_priority_processes[low_priority_count++] = pinfo;
            }

            // 비활성 프로세스(Sleeping, Stopped) 목록 추가
            if (pinfo.state == 'S' || pinfo.state == 'T')
            {
                inactive_processes[inactive_count++] = pinfo;
            }

            // 좀비 프로세스 목록 추가
            if (pinfo.state == 'Z')
            {
                zombie_processes[zombie_count++] = pinfo;
            }

            // 디버깅용 출력
            // printf("Scanned Process: %s (PID: %d), Memory: %lu kB, Priority: %d\n", pinfo.name, pinfo.pid, pinfo.memory, pinfo.priority);
        }
    }

    closedir(dir);
}

// 사용량이 많은 순서
void sort_large_consumed_processes()
{
    for (int i = 0; i < large_consumed_count - 1; i++)
    {
        for (int j = i + 1; j < large_consumed_count; j++)
        {
            if (consumed_large[j].memory > consumed_large[i].memory)
            {
                ProcessInfo temp = consumed_large[i];
                consumed_large[i] = consumed_large[j];
                consumed_large[j] = temp;
            }
        }
    }
}

// 우선순위가 낮은 프로세스를 우선순위 기준으로 정렬
void sort_low_priority_processes()
{
    for (int i = 0; i < low_priority_count - 1; i++)
    {
        for (int j = i + 1; j < low_priority_count; j++)
        {
            if (low_priority_processes[j].priority > low_priority_processes[i].priority)
            {
                ProcessInfo temp = low_priority_processes[i];
                low_priority_processes[i] = low_priority_processes[j];
                low_priority_processes[j] = temp;
            }
        }
    }
}

// 우선순위가 낮은 프로세스를 종료하는 함수
void terminate_low_priority_processes()
{
    int cursor_line = 1;
    mvprintw(cursor_line++, 1, "===== low_priority_processes =====");

    if (low_priority_count == 0)
    {
        mvprintw(cursor_line++, 1, "No low priority processes to terminate.");
        mvprintw(cursor_line, 1, "=====================================\n");
        return;
    }

    // 우선순위가 낮은 순으로 메모리가 0보다 큰 프로세스 찾기
    int found = 0;
    for (int i = 0; i < low_priority_count - 1; i++)
    {
        if (low_priority_processes[i].memory > 0)
        {
            ProcessInfo target_process = low_priority_processes[i];
            mvprintw(cursor_line++, 1, "Lowest Priority Process with Memory > 0 \n %s (PID: %d), Memory: %lu kB, Priority: %d\n", target_process.name, target_process.pid, target_process.memory,
                     target_process.priority);

            cursor_line += 3;
            mvprintw(cursor_line++, 1, "Do you want to kill this process? (y/n):");
            refresh();
            int ch = getch();
            if (ch == 'y' || ch == 'Y')
            {
                if (kill(target_process.pid, SIGTERM) == 0)
                {
                    mvprintw(cursor_line++, 1, "Terminating process: %s (PID: %d), Memory: %lu kB\n", target_process.name, target_process.pid, target_process.memory);

                    break;
                }
                else
                {
                    mvprintw(cursor_line++, 1, "Failed to terminate the process. Insufficient permissions?");
                    break;
                }
            }
            else
            {
                mvprintw(cursor_line++, 1, "Process termination canceled.");
                break;
            }

            sleep(1); // 종료 대기
            found = 1;
            break; // 종료 후 루프 탈출
        }
    }

    // 메모리가 0보다 큰 프로세스를 찾지 못한 경우
    if (!found)
    {
        mvprintw(cursor_line++, 1, "No process with Memory > 0 to terminate.");
    }
    mvprintw(cursor_line, 1, "=====================================\n");
}

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

void cpu_top()
{
    close(pipe_fd[0]); // 읽기용 닫기
    while (1)
    {

        FILE *fp = popen("ps -eo pid,comm,%cpu --sort=-%cpu --no-headers | head -n 10", "r");
        // 에러처리
        if (fp == NULL)
        {
            perror("popen failed");
            exit(1);
        }

        char buffer[2048] = {0}; // 부모 프로세스로 보낼 결과 저장
        char line[512];          // 한 줄씩 읽을 임시 버퍼
        char pid[16], command[256], cpu[32];
        int rank = 1;

        // 버퍼에 헤더 추가
        snprintf(buffer, sizeof(buffer), "Rank  COMMAND          PID      CPU(%%)\n");

        // 데이터 읽기 및 처리
        while (fgets(line, sizeof(line), fp) != NULL && rank <= 10)
        {
            // PID,CMD,CPU정보를 저장
            sscanf(line, "%15s %255s %15s", pid, command, cpu); // command 최대 크기 제한

            // 명령어가 길면 자르기-->12자만 출력하고 ...
            if (strlen(command) > 15)
            {
                snprintf(command, sizeof(command), "%.12s...", command);
            }

            int result = snprintf(line, sizeof(line), "%-5d %-16s %-8s %-8s\n", rank++, command, pid, cpu);

            // 버퍼 초과하지 않는지 확인
            if (result < 0 || result >= sizeof(line))
            {
                fprintf(stderr, "Warning\n");
                continue;
            }

            strcat(buffer, line); // line에 저장된 결과를 버퍼에 추가
        }

        pclose(fp);

        // 부모한테 전달
        write(pipe_fd[1], buffer, strlen(buffer) + 1);

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
        char result[2048] = {0}; // 배열 크기 증가
        char pid[16], command[128], mem[16];
        int rank = 1;

        strcat(result, "Rank  COMMAND          PID      Memory(Kb)  Memory(%)\n"); // % 항목 추가
        while (fgets(buffer, sizeof(buffer), fp) && rank <= 10)
        {
            sscanf(buffer, "%s %s %s", pid, command, mem);

            // %MEM 값을 확인
            float mem_percent = atof(mem);
            if (mem_percent <= 0.0)
                continue; // 메모리 사용량이 0인 경우 건너뜀

            // %MEM(Kb) 변환
            long mem_kb = (long)((mem_percent / 100.0) * sysconf(_SC_PHYS_PAGES) * sysconf(_SC_PAGE_SIZE) / 1024);

            char line[256];
            sprintf(line, "%-5d %-16s %-8s %-10ld  %-10.2f\n", rank++, command, pid, mem_kb, mem_percent);
            strcat(result, line);
        }

        pclose(fp);
        write(pipe_fd[1], result, strlen(result) + 1);
        sleep(1);
    }
}

void alarm_process()
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
        char result[2048] = {0}; // 배열 크기 증가
        char line[256];
        char pid[16], command[128], mem[16];
        int rank = 1;
        int found = 0;

        strcat(result, "Rank  COMMAND          PID      Memory(Kb)  Memory(%)\n"); // % 항목 추가
        while (fgets(buffer, sizeof(buffer), fp) && rank <= 10)
        {
            sscanf(buffer, "%s %s %s", pid, command, mem);

            // %MEM 값을 확인
            float mem_percent = atof(mem);
            if (mem_percent <= 0.0)
                continue; // 메모리 사용량이 0인 경우 건너뜀

            // %MEM(Kb) 변환
            long mem_kb = (long)((mem_percent / 100.0) * sysconf(_SC_PHYS_PAGES) * sysconf(_SC_PAGE_SIZE) / 1024);

            if (mem_percent >= 25)
            {
                sprintf(line, "%-5d %-16s %-8s %-10ld  %-10.2f\n", rank++, command, pid, mem_kb, mem_percent);
                strcat(result, line);
                found++;
            }
        }

        pclose(fp);

        if (found == 0)
        {
            sprintf(line, "There are no processes using more than 30%% of memory");
            write(pipe_fd[1], line, strlen(line) + 1);
        }

        write(pipe_fd[1], result, strlen(result) + 1);
        sleep(1);
    }
}

void serch_process()
{
    close(pipe_fd[0]);

    char status_path[300];
    snprintf(status_path, sizeof(status_path), "/proc/%s/status", pid_input);

    while (1)
    {

        FILE *fp = fopen(status_path, "r");
        if (fp == NULL)
        {
            exit(1);
        }

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
        sleep(1);
    }
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
