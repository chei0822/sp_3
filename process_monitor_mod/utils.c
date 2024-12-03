#include <curses.h>
#include "global.h"
#include <dirent.h>
#include <pthread.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/wait.h>
#include <unistd.h>
#include "utils.h"


void handle_signal(int signal) {
    (void)signal;  // 매개변수 무시
    terminate = 1;
}

int is_system_process(const char *name, int pid) {
    for (int i = 0; i < SYSTEM_PROCESS_COUNT; i++) {
        if (strcmp(name, SYSTEM_PROCESSES[i]) == 0) {
            return 1;
        }
    }
    if (pid >= 1 && pid <= 100) {
        return 1;
    }
    const char *shells[] = {"bash", "sh", "zsh", "ksh", "fish", "tcsh"};
    for (size_t i = 0; i < sizeof(shells) / sizeof(shells[0]); i++) {
        if (strcmp(name, shells[i]) == 0) {
            return 1;
        }
    }
    return 0;
}
