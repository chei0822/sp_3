#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>
#include <signal.h>
#include <dirent.h>
#include <curses.h>
#include <sys/ioctl.h>


// 전역 변수들
char option[100];
int row, col;

// 프로세스 정보를 저장할 구조체 // MEM 추가
typedef struct {
    int pid; // 프로세스 ID
    char name[256]; // 프로세스 이름
    unsigned long mem_usage; // 메모리 사용량
} ProcessInfo;

#define MAX_PROCESSES 1024 // 최대 프로세스 개수 제한 // MEM 추가

// 메모리 사용량이 높은 상위 10개 프로세스를 가져오는 함수 // MEM 추가
void get_top_memory_processes(ProcessInfo *processes, int *count){
    DIR *proc_dir = opendir("/proc");
    struct dirent *entry;
    *count = 0;

    if(!proc_dir){
        perror("opendir /proc fail");

        return;
    }

    while((entry=readdir(proc_dir)) != NULL){
        if(*count >= MAX_PROCESSES){
            break;
        }

        if(entry->d_type==DT_DIR && atoi(entry->d_name) > 0){
            char status_path[256];
            snprintf(status_path, sizeof(status_path), "/proc/%s/status", entry->d_name);

            FILE *status_file = fopen(status_path, "r");
            if(!status_file){
                continue;
            }

            ProcessInfo pinfo = {0};
            pinfo.pid = atoi(entry->d_name);

            char line[256];
            while(fgets(line, sizeof(line), status_file)){
                if(strncmp(line, "Name:", 5) == 0){
                    sscanf(line, "Name:\t%s", pinfo.name);
                } else if(strncmp(line, "VmRSS:", 6) == 0){
                    sscanf(line, "VmRSS:\t%lu", &pinfo.mem_usage);
                    
                    break;
                }
            }
            fclose(status_file);

            if(pinfo.mem_usage > 0){
                processes[(*count)++] = pinfo;
            }
        }
    }

    closedir(proc_dir);

    // 메모리 사용량이 큰 순으로 내림차순 정렬
    for(int i = 0; i < *count-1; i++){
        for(int j = i+1; j < *count; j++){
            if(processes[i].mem_usage < processes[j].mem_usage){
                ProcessInfo temp = processes[i];
                processes[i] = processes[j];
                processes[j] = temp;
            }
        }
    }
}

int main()
{
    struct winsize wbuf;

    if (ioctl(0, TIOCGWINSZ, &wbuf) != -1)
    {
        row = wbuf.ws_row;
        col = wbuf.ws_col;
    }

    pid_t pid;

    initscr(); // ncurses 초기화
    echo();    // 입력 문자를 화면에 출력
    curs_set(FALSE);

    while (1)
    {
        // clear(); // 화면 클리어
        move(1, (col / 2) - 9);       // 커서 이동
        addstr("Process Monitoring"); // 문자열 출력

        int i = 3;

        move(i++, 1);
        standout();
        addstr("Option list");
        standend();

        move(i++, 1);                                                             // 커서 이동
        addstr("CPU - View the top 10 processes with the highest CPU usage.");    // 문자열 출력
        move(i++, 1);                                                             // 커서 이동
        addstr("MEM - View the top 10 processes with the highest Memory usage."); // 문자열 출력
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
            break;
        }
        else if (strcmp(option, "CPU") != 0 && strcmp(option, "MEM") != 0 && strcmp(option, "SERCH") != 0 && strcmp(option, "CLEAN") != 0)
        {
            move(6, 5);
            addstr("check option.");
            refresh(); // 화면 갱신
        }

        pid = fork();

        if (pid == 0)
        {
            if (strcmp(option, "CPU") == 0)
            {
                // 1초 주기로 CPU 사용량 높은 순위 10개 목록을 PIPE로 부모한테 보내는 코드
            }

            if (strcmp(option, "MEM") == 0)
            {
                // 1초 주기로 memory 사용량 높은 순위 10개 목록을 PIPE로 부모한테 보내는 코드
                ProcessInfo processes[MAX_PROCESSES];
                int count;

                get_top_memory_processes(processes, &count);

                clear();
                move(1, 1);
                addstr("Top 10 processes by memory usage:");
                move(2, 1);
                addstr("    [Name]                    [PID]      [Memory(KB)]");

                for(int j = 0; j<count && j<10; j++){
                    move(3+j, 1);
                    char buffer[256];
                    snprintf(buffer, sizeof(buffer), "%2d. %-25s %-10d %-10lu", j+1, processes[j].name, processes[j].pid, processes[j].mem_usage);
                    addstr(buffer);
                }
                refresh();

                move(row-2, 1);
                addstr("Press any key to return to the menu.");
                getch();
            }

            if (strcmp(option, "SEARCH") == 0)
            {
                // 검색한 프로세스 정보를  PIPE로 부모한테 보내는 코드
            }

            if (strcmp(option, "CLEAN") == 0)
            {
                // 성민님, 여원님이 작성하신 코드를 바탕으로 불필요한(종료시킨) 프로세스 정보를  PIPE로 부모한테 보내는 코드
            }
        }
        else
        {
            // 부모 프로세스
            // 각 option에 맞는 화면 curses 구현 - while(1)
            // "q"를 입력하면 자식 프로세스를 종료시키고 break

            if (strcmp(option, "CPU") == 0)
            {
            }
            if (strcmp(option, "MEM") == 0)
            {
            }
            if (strcmp(option, "SEARCH") == 0)
            {
            }
            if (strcmp(option, "CLEAN") == 0)
            {
            }
        }
    }

    endwin(); // ncurses 종료

    printf("Exiting safely...\n");
    return 0;
}