#include <curses.h>
#include <dirent.h>
#include <pthread.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <unistd.h>

typedef struct {
  int pid;
  unsigned long memory;
  char name[256];
} ProcessInfo;

// 전역 변수들
float cpu_usage = 0.0;
float memory_usage = 0.0;
int terminate = 0;             // 프로그램 종료 플래그
ProcessInfo top_processes[10]; // 상위 10개 프로세스 저장 배열
pid_t cpu_pid, mem_pid, serch_pid,
    clean_pid; // 기능에 따른 프로세스를 담을 변수
char option[100];
int row, col;

// CPU 사용량 계산
float get_cpu_usage() {
  FILE *file;
  unsigned long long int user, nice, system, idle;
  static unsigned long long int prev_user, prev_nice, prev_system, prev_idle;
  float usage;

  file = fopen("/proc/stat", "r");
  if (file == NULL) {
    perror("Could not open /proc/stat");
    exit(1);
  }
  fscanf(file, "cpu %llu %llu %llu %llu", &user, &nice, &system, &idle);
  fclose(file);

  unsigned long long int total_diff = (user - prev_user) + (nice - prev_nice) +
                                      (system - prev_system) +
                                      (idle - prev_idle);
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
  fscanf(file, "MemTotal: %lu kB\nMemFree: %lu kB", &total_memory,
         &free_memory);
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
    if (entry->d_type == DT_DIR &&
        atoi(entry->d_name) > 0) { // 프로세스 디렉터리만 선택
      char path[300], line[300];
      snprintf(path, sizeof(path), "/proc/%s/status", entry->d_name);
      FILE *file = fopen(path, "r");
      if (file == NULL)
        continue;

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
void handle_signal(int signal) { terminate = 1; }

// 모니터링 데이터 출력 함수
void display_system_monitor() {
  initscr();
  noecho();
  curs_set(FALSE);
  nodelay(stdscr, TRUE); // 입력 없을 때 즉시 반환

  while (!terminate) {
    clear();
    mvprintw(2, 5, "CPU Usage: %.2f %%", cpu_usage);
    mvprintw(4, 5, "Memory Usage: %.2f %%", memory_usage);
    mvprintw(8, 5, "Top 10 Processes by Memory Usage:");
    for (int i = 0; i < 10; i++) {
      if (top_processes[i].memory > 0) {
        mvprintw(9 + i, 5, "%2d. %s (PID: %d) - %lu kB", i + 1,
                 top_processes[i].name, top_processes[i].pid,
                 top_processes[i].memory);
      }
    }

    int ch = getch();
    if (ch == 'q') {
      terminate = 1;
    }

    refresh();
    usleep(500000); // 0.5초 대기 (화면 깜박임 줄임)
  }

  endwin();
}

int main() {
  // SIGINT 신호 설정
  // signal(SIGINT, handle_signal);

  struct winsize wbuf;

  if (ioctl(0, TIOCGWINSZ, &wbuf) != -1) {
    row = wbuf.ws_row;
    col = wbuf.ws_col;
  }

  pid_t pid;

  initscr(); // ncurses 초기화
  echo();    // 입력 문자를 화면에 출력
  curs_set(FALSE);

  while (1) {
    // clear(); // 화면 클리어
    move(1, (col / 2) - 9);       // 커서 이동
    addstr("Process Monitoring"); // 문자열 출력

    int i = 3;

    move(i++, 1);
    standout();
    addstr("Option list");
    standend();

    move(i++, 1); // 커서 이동
    addstr(
        "CPU - View the top 10 processes with the highest CPU usage."); // 문자열
                                                                        // 출력
    move(i++, 1); // 커서 이동
    addstr(
        "MEM - View the top 10 processes with the highest Memory usage."); // 문자열
                                                                           // 출력
    move(i++, 1); // 커서 이동
    addstr(
        "SEARCH - View information about the desired process."); // 문자열 출력
    move(i++, 1);                                        // 커서 이동
    addstr("CLEAN - Terminates unnecessary processes."); // 문자열 출력
    move(i++, 1);                                        // 커서 이동
    addstr("exit - exits the program.");                 // 문자열 출력

    // 입력 칸 표시
    move(row - 2, 1);
    addstr("option:");
    standout();
    for (int i = 0; i < col - 10; i++) {
      addstr(" ");
    }
    standend();

    refresh(); // 화면 갱신

    move(row - 2, 9);
    standout();
    getstr(option);
    standend();

    if (strcmp(option, "exit") == 0) {
      break;
    } else if (strcmp(option, "CPU") != 0 && strcmp(option, "MEM") != 0 &&
               strcmp(option, "SERCH") != 0 && strcmp(option, "CLEAN") != 0) {
      move(6, 5);
      addstr("check option.");
      refresh(); // 화면 갱신
    }

    pid = fork();

    if (pid == 0) {
      if (strcmp(option, "CPU") == 0) {
        // 1초 주기로 CPU 사용량 높은 순위 10개 목록을 PIPE로 부모한테 보내는
        // 코드
      }
      if (strcmp(option, "MEM") == 0) {
        // 1초 주기로 memory 사용량 높은 순위 10개 목록을 PIPE로 부모한테 보내는
        // 코드
      }
      if (strcmp(option, "SERCH") == 0) {
        // 검색한 프로세스 정보를  PIPE로 부모한테 보내는 코드
      }
      if (strcmp(option, "CLEAN") == 0) {
        // 성민님, 여원님이 작성하신 코드를 바탕으로 불필요한(종료시킨) 프로세스
        // 정보를  PIPE로 부모한테 보내는 코드
      }
    } else {
      // 부모 프로세스
      // 각 option에 맞는 화면 curses 구현 - while(1)
      // "q"를 입력하면 자식 프로세스를 종료시키고 break

      if (strcmp(option, "CPU") == 0) {
      }
      if (strcmp(option, "MEM") == 0) {
      }
      if (strcmp(option, "SERCH") == 0) {
      }
      if (strcmp(option, "CLEAN") == 0) {
      }
    }
  }

  endwin(); // ncurses 종료

  printf("Exiting safely...\n");
  return 0;
}
