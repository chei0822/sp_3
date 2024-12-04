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
#include "global.h"
#include "utils.h"
#include "ui.h"

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

    // 결과 문자열 형식화
    int pad_width = (col - 50) / 2; // 중앙 정렬을 위한 계산

    int cursor_line = 1;
    mvprintw(cursor_line++, pad_width, "========= low_priority_processes =========");

    if (low_priority_count == 0)
    {
        mvprintw(cursor_line++, 1, "No low priority processes to termin.");
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

            cursor_line += 1;
            mvprintw(cursor_line++, pad_width + 3, "Lowest Priority Process with Memory > 0 \n");
            mvprintw(cursor_line++, pad_width - 8, "----------------------------------------------------------");
            mvprintw(cursor_line++, pad_width - 7, "%s (PID: %d), Memory: %lu kB, Priority: %d\n", target_process.name, target_process.pid, target_process.memory, target_process.priority);
            mvprintw(cursor_line++, pad_width - 8, "----------------------------------------------------------");

            cursor_line += 3;
            mvprintw(row - 2, 1, "Do you want to kill this process? (y/n):");
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
                    attron(COLOR_PAIR(1));
                    mvprintw(cursor_line++, 1, "Failed to termin the process. Insufficient permissions?");
                    attron(COLOR_PAIR(1));
                    break;
                }
            }
            else
            {
                attron(COLOR_PAIR(1));
                mvprintw(cursor_line++, 1, "Process termination canceled.");
                attroff(COLOR_PAIR(1));
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
        attron(COLOR_PAIR(1));
        mvprintw(cursor_line++, 1, "No process with Memory > 0 to termin.");
        attroff(COLOR_PAIR(1));
    }
}

void cpu_top() {
    close(pipe_fd[0]); // 읽기용 파이프 닫기
    while (1) {
        FILE *fp = popen("ps -eo pid,comm,%cpu --sort=-%cpu --no-headers | head -n 10", "r");
        if (fp == NULL) {
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
        while (fgets(line, sizeof(line), fp) != NULL && rank <= 10) {
            // PID, CMD, CPU 정보를 읽음
            sscanf(line, "%15s %255s %15s", pid, command, cpu);

            // 명령어가 길면 자르기
            char temp[256];
            snprintf(temp, sizeof(temp), "%.12s...", command); // 임시 버퍼 사용
            strncpy(command, temp, sizeof(command) - 1);
            command[sizeof(command) - 1] = '\0';

            // 출력 결과를 버퍼에 추가
            int result = snprintf(line, sizeof(line), "%-5d %-16s %-8s %-8s\n", rank++, command, pid, cpu);
            if (result < 0 || (size_t)result >= sizeof(line)) { // 서명 문제 해결
                continue;
            }

            strcat(buffer, line); // 결과를 메인 버퍼에 추가
        }

        pclose(fp);

        // 부모에게 전달
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
