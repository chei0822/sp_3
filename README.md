# sp_3
main.c:
사용자 입력에 따라 기능(CPU, MEM, SEARCH 등)을 호출.

process_handler.c:
프로세스 관련 작업을 처리.
프로세스 정보 수집, 정렬, 종료, 좀비 프로세스 요약 등을 포함.
예: get_process_info, terminate_low_priority_processes

ui.c:
UI
메뉴 표시, 결과 출력 등 화면 처리.
예: display_main_menu.
utils.c:
공통적으로 사용되는 함수 모음
예: handle_signal, is_system_process.

global.h:
모든 코드에서 공유되는 전역 변수 및 구조체 정의.
전역 변수: option, pid_input, consumed_large[]..
구조체: ProcessInfo.
