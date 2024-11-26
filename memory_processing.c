#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>
#include <dirent.h>

typedef struct {
    int pid;
    unsigned long memory;
    int priority;
    char name[256];
} ProcessInfo;

ProcessInfo low_priority_processes[1024]; // 우선순위 낮은 프로세스 저장 배열
int low_priority_count = 0; // 우선순위 낮은 프로세스 개수

// 프로세스 정보 수집 함수
void get_process_info() {
    DIR *dir = opendir("/proc");
    struct dirent *entry;
    low_priority_count = 0;

    if (dir == NULL) {
        perror("Could not open /proc directory");
        return;
    }

    while ((entry = readdir(dir)) != NULL) {
        if (entry->d_type == DT_DIR && atoi(entry->d_name) > 0) { // 프로세스 디렉터리만 선택
            char status_path[300], stat_path[300], line[300];
            snprintf(status_path, sizeof(status_path), "/proc/%s/status", entry->d_name);
            snprintf(stat_path, sizeof(stat_path), "/proc/%s/stat", entry->d_name);

            ProcessInfo pinfo;
            pinfo.pid = atoi(entry->d_name);
            pinfo.memory = 0;
            pinfo.priority = 0;

            // 메모리 정보 읽기
            FILE *status_file = fopen(status_path, "r");
            if (status_file != NULL) {
                while (fgets(line, sizeof(line), status_file)) {
                    if (strncmp(line, "Name:", 5) == 0) {
                        sscanf(line, "Name: %s", pinfo.name);
                    }
                    if (strncmp(line, "VmRSS:", 6) == 0) {
                        sscanf(line, "VmRSS: %lu kB", &pinfo.memory);
                    }
                }
                fclose(status_file);
            }

            // Priority 정보 읽기
            FILE *stat_file = fopen(stat_path, "r");
            if (stat_file != NULL) {
                if (fgets(line, sizeof(line), stat_file)) {
                    char comm[256];
                    int dummy;
                    sscanf(line, "%d %s %*c ""%d %*d %*d %*d %*d %*d %*u %*u %*u %*u %*u %*u %*u %*d %d", &pinfo.pid, comm, &dummy, &pinfo.priority);
                }
                fclose(stat_file);
            }

            // 조건을 만족하는 프로세스만 추가
            if (pinfo.pid > 1) { 
                low_priority_processes[low_priority_count++] = pinfo;
            }

            // 디버깅용 출력
            // printf("Scanned Process: %s (PID: %d), Memory: %lu kB, Priority: %d\n", pinfo.name, pinfo.pid, pinfo.memory, pinfo.priority);
        }
    }
    
    closedir(dir);
}

 
// 우선순위가 낮은 프로세스를 우선순위 기준으로 정렬
void sort_low_priority_processes() {
    for (int i = 0; i < low_priority_count - 1; i++) {
        for (int j = i + 1; j < low_priority_count; j++) {
            if (low_priority_processes[j].priority > low_priority_processes[i].priority) { 
                ProcessInfo temp = low_priority_processes[i];
                low_priority_processes[i] = low_priority_processes[j];
                low_priority_processes[j] = temp;
            }
        }
    }
}

// 우선순위가 낮은 프로세스를 종료하는 함수
void terminate_low_priority_processes() {
    if (low_priority_count == 0) {
        printf("No low priority processes to terminate.\n");

        return;
    }

    // 우선순위가 낮은 순으로 메모리가 0보다 큰 프로세스 찾기
    int found = 0;
    for (int i = 0; i < low_priority_count - 1; i++) {
        if (low_priority_processes[i].memory > 0) {
            ProcessInfo target_process = low_priority_processes[i];
            printf("Lowest Priority Process with Memory > 0: %s (PID: %d), Memory: %lu kB, Priority: %d\n", target_process.name, target_process.pid, target_process.memory, target_process.priority);

            // 프로세스 종료
            printf("Terminating process: %s (PID: %d), Memory: %lu kB\n", target_process.name, target_process.pid, target_process.memory);
            kill(target_process.pid, SIGTERM); // 프로세스 종료
            
            sleep(1); // 종료 대기
            found = 1;
            break; // 종료 후 루프 탈출
        }
    }

    // 메모리가 0보다 큰 프로세스를 찾지 못한 경우
    if (!found) {
        printf("No process with Memory > 0 to terminate.\n");
    }
}

int main() {
    // 1. 프로세스 정보 수집
    get_process_info();

    // 2. 우선순위가 낮은 프로세스를 정렬
    sort_low_priority_processes();

    // 3. 우선순위가 낮은 프로세스를 종료
    terminate_low_priority_processes();

    return 0;
}