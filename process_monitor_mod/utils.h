#ifndef UTILS_H
#define UTILS_H

#include "global.h"

// 유틸리티 함수
void handle_signal(int signal);          // 신호 처리
int is_system_process(const char *name, int pid); // 시스템 프로세스 확인

#endif
