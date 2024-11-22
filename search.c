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

        int pipefd[2];
        if (pipe(pipefd) == -1) { 
            perror("pipe");
            exit(EXIT_FAILURE);
        }

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
        //자식프로세스
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
            if (strcmp(option, "SEARCH") == 0||strcmp(option,"search"))
            {
                close(pipefd[0]);
                char search_query[100];
                printf("Enter process name or PID to search: ");
                scanf("%s", search_query);
                char command[200];
                sprintf(command, "ps -e -o pid,comm,pcpu,pmem | grep %s", search_query);

                FILE *fp = popen(command, "r");
                if (!fp)
                {
                    perror("popen");
                    exit(EXIT_FAILURE);
                }
                 char buffer[1024];

                while (fgets(buffer, sizeof(buffer), fp))
                {
                    write(pipefd[1], buffer, strlen(buffer)); // PIPE로 데이터 전송
                }

                pclose(fp);
                close(pipefd[1]); // 쓰기 닫기
                exit(0);
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
                // 각 option에 맞는 화면 curses 구현 - while(1)
		        // "q"를 입력하면 자식 프로세스를 종료시키고 break
                close(pipefd[1]); // 쓰기 닫기

                char buffer[1024];
                int bytes_read;

                while ((bytes_read = read(pipefd[0], buffer, sizeof(buffer) - 1)) > 0)
                {
                    buffer[bytes_read] = '\0'; // null-terminate the string
                    move(8, 1);
                    addstr("Search Results:");
                    move(9, 1);
                    addstr(buffer); // 검색 결과 출력
                    refresh();
                }

                close(pipefd[0]); // 읽기 닫기

                move(12, 1);
                addstr("Press 'q' to return to the main menu.");
                refresh();

                char ch;
                while ((ch = getch()) != 'q')
                    ;
            
            }
            if (strcmp(option, "CLEAN") == 0)
            {
            }
        }   

            endwin(); // ncurses 종료

            printf("Exiting safely...\n");
            return 0;
    }
}