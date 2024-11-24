#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>
#include <signal.h>
#include <dirent.h>
#include <curses.h>

#define REFRESH_INTERVAL 1 // 기본 갱신 주기 (초 단위)
#define MEMORY_THRESHOLD 80.0 // 메모리 알람 임계치 (%) // 추가

typedef struct {
    int pid;
    unsigned long memory;
    char name[256];
} ProcessInfo;

// 전역 변수들
float cpu_usage = 0.0;
float memory_usage = 0.0;
int refresh_rate = REFRESH_INTERVAL; // 사용자 지정 갱신 주기
int terminate = 0; // 프로그램 종료 플래그
ProcessInfo top_processes[10]; // 상위 10개 프로세스 저장 배열

// CPU 사용량 계산
float get_cpu_usage() {
    FILE *file;
    unsigned long long int user, nice, system, idle;
    static unsigned long long int prev_user, prev_nice, prev_system, prev_idle;
    float usage;

    // 저 파일에는 Cpu    user   system    nice    idle    wait    hi    si    zero 이렇게 저장되어 있음
    // user, system, nice, idle 값만 쓴다. 
    file = fopen("/proc/stat", "r");
    if (file == NULL) {
        perror("Could not open /proc/stat");
        exit(1);
    }
    fscanf(file, "cpu %llu %llu %llu %llu", &user, &nice, &system, &idle);
    fclose(file);

    unsigned long long int total_diff = (user - prev_user) + (nice - prev_nice) +
                                        (system - prev_system) + (idle - prev_idle);
    //cpu 전체 값
    unsigned long long int idle_diff = idle - prev_idle;

    usage = 100.0 * (1.0 - ((float)idle_diff / (float)total_diff));

    prev_user = user;
    prev_nice = nice;
    prev_system = system;
    prev_idle = idle;

    return usage;
}

// 메모리 사용량 계산
float get_memory_usage() {
    FILE *file;
    unsigned long total_memory, free_memory;
    float usage;
    
    file = fopen("/proc/meminfo", "r");
    if (file == NULL) {
        perror("Could not open /proc/meminfo");
        exit(1);
    }
    //파일에 어떻게 저장되어 있냐면 
    //MemTotal : __ kB\n MemFree: __ kB 이 두개 만 쓸거다..
    //MemTotal - 전체 물리 메모리 크기
    //MemFree - 사용 가능한 메모리 크기 
    fscanf(file, "MemTotal: %lu kB\nMemFree: %lu kB", &total_memory, &free_memory);
    fclose(file);

    usage = 100.0 * (1.0 - ((float)free_memory / (float)total_memory));

    return usage;
}

// 메모리 사용량 상위 10개 프로세스 정보 수집
void get_top_memory_processes() {
    DIR *dir = opendir("/proc");
    struct dirent *entry;
    ProcessInfo processes[1024];
    int count = 0;

    if (dir == NULL) {
        perror("Could not open /proc directory");
        return;
    }

    while ((entry = readdir(dir)) != NULL) {
        if (entry->d_type == DT_DIR && atoi(entry->d_name) > 0) { // 프로세스 디렉터리만 선택
            char path[300], line[300];
            snprintf(path, sizeof(path), "/proc/%s/status", entry->d_name);
            FILE *file = fopen(path, "r");
            if (file == NULL) continue;

            ProcessInfo pinfo;
            pinfo.pid = atoi(entry->d_name);
            pinfo.memory = 0;

            while (fgets(line, sizeof(line), file)) {
                if (strncmp(line, "Name:", 5) == 0) {
                    sscanf(line, "Name: %s", pinfo.name);
                }
                if (strncmp(line, "VmRSS:", 6) == 0) {
                    sscanf(line, "VmRSS: %lu kB", &pinfo.memory);
                    break;
                }
            }
            fclose(file);

            processes[count++] = pinfo;
        }
    }
    closedir(dir);

    // 메모리 사용량 기준으로 상위 10개 프로세스 정렬
    for (int i = 0; i < count - 1 && i < 10; i++) {
        for (int j = i + 1; j < count; j++) {
            if (processes[j].memory > processes[i].memory) {
                ProcessInfo temp = processes[i];
                processes[i] = processes[j];
                processes[j] = temp;
            }
        }
    }

    // 상위 10개만 저장
    for (int i = 0; i < 10 && i < count; i++) {
        top_processes[i] = processes[i];
    }
}

// PID 검색 결과 출력 및 종료 여부 확인 함수
void display_process_memory_usage_and_prompt(int pid) {
    char status_path[256];
    snprintf(status_path, sizeof(status_path), "/proc/%d/status", pid);
    // /prc/[pid]/status 해당 pid에 대한 프로세스 정보를 가지고 있음 
    // 해당 pid에 대한 
    if (access(status_path, F_OK) != 0) 
    {
        mvprintw(12, 5, "PID %d: Not Found.", pid);
        refresh();
        return;
    }
    //access(파일, 파일을 어떤 mode로 접근 할 것인가)
    FILE *status_file = fopen(status_path, "r");
    if (status_file == NULL) 
    {
        mvprintw(12, 5, "PID %d: Access Denied.", pid);
        refresh();
        return;
    }

    char line[256];
    int found = 0;
    int display_line = 12;

    while (fgets(line, sizeof(line), status_file)) 
    {
        if (strncmp(line, "VmSize:", 7) == 0 || strncmp(line, "VmRSS:", 6) == 0) {
            mvprintw(display_line++, 5, "%s", line);
            found = 1;
        }
    }

    if (!found) 
    {
        mvprintw(display_line, 5, "PID %d: No Memory Info Available.", pid);
        display_line++;
    }
    fclose(status_file);

    mvprintw(display_line + 1, 5, "Do you want to terminate this process? (y/n): ");
    refresh();

    //SIGTERM이 지울 수 있는지 확인받는 signal이라 안지워지는 거는 자동으로 Process termination cancelled. 가 뜬다.
    int ch = getch();  
    if (ch == 'y' || ch == 'Y') {
        if (kill(pid, SIGTERM) == 0) {
            mvprintw(display_line + 2, 5, "Process %d terminated successfully.", pid);
        } else {
            mvprintw(display_line + 2, 5, "Failed to terminate process %d.", pid);
        }
    } else {
        mvprintw(display_line + 2, 5, "Process termination cancelled.");
    }
    refresh();

    
}


// PID 입력 함수
void prompt_for_pid() {
    nodelay(stdscr, FALSE); //blocking mode
    echo(); // 입력이 보임

    int pid;
    mvprintw(21, 5, "Enter PID to Search: ");
    refresh();
    scanw("%d", &pid);

    noecho(); //입력이 안보임
    cbreak(); //라인 버퍼 모드 해제(입력하는대로 전달)
    nodelay(stdscr, TRUE); //nonblocking mode

    clear();
    display_process_memory_usage_and_prompt(pid);

    mvprintw(22, 5, "Press Enter to return to monitoring.");
    refresh();

    //엔터를 치면 끝..
    while(1)
    {
        if(getch() == '\n')
            return;
    }    
}

// 자원 모니터링 스레드 함수
void *monitor_resources(void *arg) {
    while (!terminate) {
        cpu_usage = get_cpu_usage();
        memory_usage = get_memory_usage();
        get_top_memory_processes();
        sleep(refresh_rate);
    }
    return NULL;
}

// SIGINT 신호 핸들러
void handle_signal(int signal) {
    terminate = 1;
}

// 모니터링 데이터 출력 함수
void display_system_monitor() {
    initscr();
    noecho();
    curs_set(FALSE);
    nodelay(stdscr, TRUE);

    while (!terminate) {
        clear();
        mvprintw(2, 5, "CPU Usage: %.2f %%", cpu_usage);
        mvprintw(4, 5, "Memory Usage: %.2f %%", memory_usage);

        if (memory_usage >= MEMORY_THRESHOLD) {
            attron(A_BOLD | A_BLINK);
            mvprintw(6, 5, "Warning: Memory usage exceeds %.0f%%!", MEMORY_THRESHOLD);
            attroff(A_BOLD | A_BLINK);
        }

        mvprintw(8, 5, "Current Refresh Rate: %d seconds", refresh_rate);
        mvprintw(10, 5, "Top 10 Processes by Memory Usage:");
        for (int i = 0; i < 10; i++) {
            if (top_processes[i].memory > 0) {
                mvprintw(11 + i, 5, "%2d. %s (PID: %d) - %lu kB",
                         i + 1, top_processes[i].name, top_processes[i].pid, top_processes[i].memory);
            }
        }

        mvprintw(20, 5, "Press 'p' to enter PID, 'q' to quit.");

        int ch = getch();
        if (ch == 'q') {
            terminate = 1;
        } else if (ch == 'p') {
            prompt_for_pid();
        }

        refresh();
        usleep(500000);
    }
    endwin();
}

int main() {
    signal(SIGINT, handle_signal);

    pthread_t monitor_thread;
    pthread_create(&monitor_thread, NULL, monitor_resources, NULL);

    display_system_monitor();

    pthread_join(monitor_thread, NULL);

    printf("Exiting safely...\n");
    return 0;
}
