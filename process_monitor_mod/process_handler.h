#ifndef PROCESS_HANDLER_H
#define PROCESS_HANDLER_H

#include "global.h"

// 프로세스 정보 수집 및 처리 함수
void get_process_info();
void sort_large_consumed_processes();
void sort_low_priority_processes();
void terminate_low_priority_processes();
void cpu_top();       // CPU 사용량 상위 프로세스 처리
void memory_top();    // 메모리 사용량 상위 프로세스 처리
void serch_process(); // 특정 PID 프로세스 정보 검색
void alarm_process(); // 메모리 사용량 알람 처fl
#endif
