#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>
#include <signal.h>
#include <dirent.h>
#include <curses.h>
#include <sys/ioctl.h>
#include <ctype.h>
#include <sys/stat.h>


// 전역 변수들
char option[100];
int row, col;

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
        int pipefd[2];
        pipe(pipefd);
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
            }
            if (strcmp(option, "SEARCH") == 0)
            {
                close(pipefd[0]);

                char check_search[100];
                move(row - 2, 1);
                addstr("Enter process name or PID to search:");
                standout();
                echo();
                move(row - 1, 1);
                getstr(check_search);
                standend();
                noecho();
                refresh();

                clear();
                move(1, (col / 2) - 9);
                addstr("Search Process Information"); // 제목 출력
                refresh();

                //proc 파일은 활성 프로세스 및 스레드에 대한 상태 정보가 있다.
                FILE *fp=fopen("/proc","r");

                if(fp==NULL)
                {
                    perror("Failed to open /proc");
                    exit(1);
                }
                struct dirent *dirent_ptr;
                DIR *dir=opendir("/proc");
                if(dir==NULL)
                {
                    perror("Failed to open /proc directory");
                    exit(1);
                }

                char result[1024]="";
                while((dirent_ptr=readdir(dir))!=NULL)
                {
                    char path[300];
                    snprintf(path, sizeof(path), "/proc/%s", dirent_ptr->d_name);

                    struct stat st;
                    if(stat(path,&st)==0 && S_ISDIR(st.st_mode))
                    {
                        if(isdigit(dirent_ptr->d_name[0]))
                        {
                            char status_path[300];
                            snprintf(status_path, sizeof(status_path), "/proc/%s/status", dirent_ptr->d_name);

                            FILE *fp1 = fopen(status_path, "r");
                            if (fp1 != NULL)
                            {
                                char line[256];
                                while (fgets(line, sizeof(line), fp1))
                                {
                                    if (strncmp(line, "Name:", 5) == 0 || strncmp(line, "Pid:", 4) == 0)
                                    {
                                        if (strstr(line, check_search) != NULL)
                                        {
                                            strcat(result, line);
                                        }
                                    }
                                }
                                fclose(fp1);
                            }
                        }
                    }
                }
                closedir(dir);
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
            if (strcmp(option, "SERCH") == 0)
            {
                close(pipefd[1]);

                char buffer[1024];
                int n_read=read(pipefd[0],buffer,sizeof(buffer)-1);

                if(n_read>0)
                {
                    buffer[n_read]='\0';
                    clear();
                    move(3, 1);
                    addstr("Search Results:");
                    move(5, 1);
                    addstr(buffer);
                    refresh();
                }
                close(pipefd[0]);
                move(row - 2, 1);
                addstr("Press 'q' to return to the main menu.");
                refresh();
                while(1)
                {
                    int ch=getch();
                    if(ch=='q'||ch=='Q')
                    {
                        kill(pid,SIGTERM);
                        break;
                    }
                }
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