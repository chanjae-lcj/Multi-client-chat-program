#include<stdio.h>
#include<stdlib.h>
#include<unistd.h>
#include<string.h>
#include<arpa/inet.h>
#include<pthread.h>
#include<time.h>

#define BUF_SIZE 100
#define NORMAL_SIZE 20

// 쓰레드 함수 프로토타입
void* send_msg(void* arg);
void* recv_msg(void* arg);
void error_handling(char* msg);
void menu();
void menuOptions();
void updateMenu();

// 전역 변수
char msg[BUF_SIZE];
char name[NORMAL_SIZE] = "[DEFAULT]";
char serv_time[NORMAL_SIZE];           // 서버 시간
char serv_port[NORMAL_SIZE];           // 서버 포트 번호
char clnt_ip[NORMAL_SIZE];             // 클라이언트 IP 주소

int mode = 1; // 1: 채팅 모드, 0: 메뉴 모드
int sock; // 전역 변수로 변경
int current_room = 1; // 현재 채팅방 번호

int main(int argc, char *argv[])
{
    struct sockaddr_in serv_addr;
    pthread_t snd_thread, rcv_thread;
    void* thread_return;

    if (argc != 4)
    {
        printf(" Usage : %s <ip> <port> <name>\n", argv[0]);
        exit(1);
    }

    /** 현재 시간 설정 **/
    struct tm *t;
    time_t timer = time(NULL);
    t = localtime(&timer);
    sprintf(serv_time, "%d-%d-%d %d:%d", t->tm_year + 1900, t->tm_mon + 1, t->tm_mday, t->tm_hour, t->tm_min);

    // 클라이언트 이름, IP 및 포트 번호 설정
    sprintf(name, "[%s]", argv[3]);
    sprintf(clnt_ip, "%s", argv[1]);
    sprintf(serv_port, "%s", argv[2]);
    sock = socket(PF_INET, SOCK_STREAM, 0);

    memset(&serv_addr, 0, sizeof(serv_addr));
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_addr.s_addr = inet_addr(argv[1]);
    serv_addr.sin_port = htons(atoi(argv[2]));

    if (connect(sock, (struct sockaddr*)&serv_addr, sizeof(serv_addr)) == -1)
        error_handling(" connect() error");

    // 초기 메뉴 출력
    menu();

    // 클라이언트 이름 전송
    write(sock, name, strlen(name));

    // 송수신 쓰레드 생성
    pthread_create(&snd_thread, NULL, send_msg, (void*)&sock);
    pthread_create(&rcv_thread, NULL, recv_msg, (void*)&sock);
    pthread_join(snd_thread, &thread_return);
    pthread_join(rcv_thread, &thread_return);
    close(sock);
    return 0;
}

// 메시지 전송을 담당하는 쓰레드 함수
void* send_msg(void* arg)
{
    int sock = *((int*)arg);
    char name_msg[NORMAL_SIZE + BUF_SIZE];
    char myInfo[BUF_SIZE];

    // 채팅방 참여 메시지 출력
    printf(" >> join the chat !! \n");
    sprintf(myInfo, "%s's join. IP_%s\n", name, clnt_ip);
    write(sock, myInfo, strlen(myInfo));

    while (1)
    {
        // 메뉴 모드 처리
        if (mode == 0) {
            menuOptions();
        } else {
            fgets(msg, BUF_SIZE, stdin);

            // 프로그램 종료 처리
            if (!strcmp(msg, "/q\n") || !strcmp(msg, "/Q\n"))
            {
                close(sock);
                exit(0);
            }

            // 메뉴 모드 전환
            if (!strcmp(msg, "/m\n") || !strcmp(msg, "/M\n")) {
                mode = 0; // switch to menu mode
                continue;
            }

            // 메시지 전송
            sprintf(name_msg, "%s %s", name, msg);
            write(sock, name_msg, strlen(name_msg));
        }
    }
    return NULL;
}

// 메시지 수신을 담당하는 쓰레드 함수
void* recv_msg(void* arg)
{
    int sock = *((int*)arg);
    char name_msg[NORMAL_SIZE + BUF_SIZE];
    int str_len;

    while (1)
    {
        str_len = read(sock, name_msg, NORMAL_SIZE + BUF_SIZE - 1);
        if (str_len == -1)
            return (void*)-1;
        name_msg[str_len] = 0;
        fputs(name_msg, stdout);

        // 방 이동 메시지 처리
        if (strstr(name_msg, "You moved to room") != NULL) {
            sscanf(name_msg, "You moved to room %d", &current_room);
            updateMenu();
        }
    }
    return NULL;
}

// 초기 메뉴 출력 함수
void menu()
{
    system("clear");
    printf(" <<<< Chat Client >>>>\n");
    printf(" Server Port : %s \n", serv_port);
    printf(" Client IP   : %s \n", clnt_ip);
    printf(" Chat Name   : %s \n", name);
    printf(" Server Time : %s \n", serv_time);
    printf(" Current Room: %d \n", current_room);
    printf(" ============= Mode =============\n");
    printf(" /m & /M. Select mode\n");
    printf(" 1. Change name\n");
    printf(" 2. Clear/Update\n");
    printf(" 3. Random Game\n");
    printf(" 4. Move Room\n");
    printf(" ================================\n");
    printf(" Exit -> /q & /Q\n\n");
}

// 메뉴 옵션 선택 함수
void menuOptions()
{
    int option;
    printf("Select mode: ");
    scanf("%d", &option);
    getchar(); // 엔터 키 입력 처리
    switch (option)
    {
        case 1: {
            char new_name[NORMAL_SIZE];
            printf("Enter new name: ");
            fgets(new_name, NORMAL_SIZE, stdin);
            new_name[strcspn(new_name, "\n")] = 0; // 개행 문자 제거

            char name_change_msg[BUF_SIZE];
            sprintf(name_change_msg, "%s has changed their name to [%s]\n", name, new_name);
            sprintf(name, "[%s]", new_name);
            write(sock, name_change_msg, strlen(name_change_msg));
            break;
        }
        case 2:
            updateMenu();
            break;
        case 3:
            sprintf(msg, "/game");
            write(sock, msg, strlen(msg));
            break;
        case 4: {
            int new_room;
            printf("Enter room number to move (1-3): ");
            scanf("%d", &new_room);
            getchar(); // 엔터 키 입력 처리
            if (new_room < 1 || new_room > 3) {
                printf("Invalid room number. Please enter a number between 1 and 3.\n");
                break;
            }
            sprintf(msg, "/move %d", new_room);
            write(sock, msg, strlen(msg));
            break;
        }
        default:
            printf("Invalid option.\n");
            break;
    }
    mode = 1; // 채팅 모드로 전환
}

// 메뉴 업데이트 함수
void updateMenu()
{
    system("clear");
    printf(" <<<< Chat Client >>>>\n");
    printf(" Server Port : %s \n", serv_port);
    printf(" Client IP   : %s \n", clnt_ip);
    printf(" Chat Name   : %s \n", name);
    printf(" Server Time : %s \n", serv_time);
    printf(" Current Room: %d \n", current_room);
    printf(" ============= Mode =============\n");
    printf(" /m & /M. Select mode\n");
    printf(" 1. Change name\n");
    printf(" 2. Clear/Update\n");
    printf(" 3. Random Game\n");
    printf(" 4. Move Room\n");
    printf(" ================================\n");
    printf(" Exit -> /q & /Q\n\n");
}

// 오류 처리 함수
void error_handling(char* msg)
{
    fputs(msg, stderr);
    fputc('\n', stderr);
    exit(1);
}
