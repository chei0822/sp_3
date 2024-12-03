#ifndef GLOBAL_H
#define GLOBAL_H

// 전역 변수 선언
extern char option[100];
extern char pid_input[100];
extern int row, col;
extern int pipe_fd[2];    // 파이프 생성용
extern int terminate;     // 프로그램 종료 플래그

// 데이터 구조 정의
typedef struct {
    int pid;
    unsigned long memory;
    char command[256];
    int priority;
    char state;
    char name[256];
} ProcessInfo;

// 전역 배열 선언
extern ProcessInfo consumed_large[1024];
extern ProcessInfo low_priority_processes[1024];
extern int large_consumed_count;
extern int low_priority_count;

// 비활성화된 프로세스 저장할 배열
extern ProcessInfo inactive_processes[1024];
extern int inactive_count;
// 좀비 프로세스 저장할 배열
extern ProcessInfo zombie_processes[1024];
extern int zombie_count;

// 시스템 프로세스 목록 및 개수
extern const char *SYSTEM_PROCESSES[];
extern const int SYSTEM_PROCESS_COUNT;

#endif
