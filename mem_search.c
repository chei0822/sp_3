#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// 특정 프로세스의 메모리 사용량 확인 함수

int main() {
    int pid;
    printf("메모리 사용량을 확인할 프로세스 ID (PID)를 입력하세요: ");
    scanf("%d", &pid);

    printf("PID %d의 메모리 사용량:\n", pid);
    char status_path[256];
    snprintf(status_path, sizeof(status_path), "/proc/%d/status", pid);

    FILE *status_file = fopen(status_path, "r");
    if (status_file == NULL) {
        perror("프로세스 상태 파일 열기 실패");
        return;
    }

    char line[256];
    while (fgets(line, sizeof(line), status_file)) {
        if (strncmp(line, "VmSize:", 7) == 0 || strncmp(line, "VmRSS:", 6) == 0) {
            printf("%s", line);
        }
    }

    fclose(status_file);

    return 0;
}
