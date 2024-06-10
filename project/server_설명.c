// 서버 프로그램을 작성하기 위한 헤더 파일 포함
#include<stdio.h>
#include<stdlib.h>
#include<unistd.h>
#include<string.h>
#include<arpa/inet.h>
#include<sys/socket.h>
#include<netinet/in.h>
#include<pthread.h>
#include<time.h>

#define BUF_SIZE 100
#define MAX_CLNT 100
#define NAME_SIZE 20
#define MAX_ROOMS 3  // 최대 방 수를 3개로 설정

// 클라이언트 요청을 처리하는 함수
void * handle_clnt(void *arg);
// 메시지를 특정 방의 모든 클라이언트에게 전송하는 함수
void send_msg(char *msg, int len, int room);
// 오류 발생 시 프로그램을 종료하는 함수
void error_handling(char *msg);
// 서버 상태를 반환하는 함수
char* serverState(int count);
// 서버 시작 시 메뉴를 출력하는 함수
void menu(char port[]);
// 랜덤 게임을 처리하는 함수
void handle_random_game(int clnt_sock, int room);
// 클라이언트의 방을 변경하는 함수
void change_room(int clnt_sock, int new_room);

/****************************/

// 각 방의 클라이언트 수를 저장하는 배열
int clnt_cnt[MAX_ROOMS] = {0}; 
// 각 방의 클라이언트 소켓을 저장하는 배열
int clnt_socks[MAX_ROOMS][MAX_CLNT];
// 클라이언트 이름을 저장하는 배열
char clnt_names[MAX_CLNT][NAME_SIZE];  
// 클라이언트가 속한 방 번호를 저장하는 배열
int clnt_rooms[MAX_CLNT];  
// 뮤텍스 객체
pthread_mutex_t mutx;

int main(int argc, char *argv[])
{
    int serv_sock, clnt_sock;
    struct sockaddr_in serv_adr, clnt_adr;
    int clnt_adr_sz;
    pthread_t t_id;
    struct tm *t;
    time_t timer;

    if (argc != 2) {
        printf(" Usage : %s <port> \n", argv[0]);
        exit(1);
    }

    pthread_mutex_init(&mutx, NULL);
    serv_sock = socket(PF_INET, SOCK_STREAM, 0);

    memset(&serv_adr, 0, sizeof(serv_adr));
    serv_adr.sin_family = AF_INET;
    serv_adr.sin_addr.s_addr = htonl(INADDR_ANY);
    serv_adr.sin_port = htons(atoi(argv[1]));

    if (bind(serv_sock, (struct sockaddr*)&serv_adr, sizeof(serv_adr)) == -1)
        error_handling(" bind() error ");
    if (listen(serv_sock, 5) == -1)
        error_handling(" listen() error ");

    menu(argv[1]);
    while (1)
    {
        timer = time(NULL); // 현재 시간 가져오기
        t = localtime(&timer); // 지역 시간으로 변환
        clnt_adr_sz = sizeof(clnt_adr);
        clnt_sock = accept(serv_sock, (struct sockaddr*)&clnt_adr, &clnt_adr_sz);

        pthread_create(&t_id, NULL, handle_clnt, (void*)&clnt_sock);
        pthread_detach(t_id);
        printf(" Connected client IP : %s ", inet_ntoa(clnt_adr.sin_addr));
        printf("(%d-%d-%d %d:%d)\n", t->tm_year + 1900, t->tm_mon + 1, t->tm_mday,
            t->tm_hour, t->tm_min);
    }
    close(serv_sock);
    return 0;
}

// 클라이언트 요청을 처리하는 함수
void *handle_clnt(void *arg)
{
    int clnt_sock = *((int*)arg);
    int str_len = 0, i, room = 0;
    char msg[BUF_SIZE];
    int idx = 0;  // 클라이언트 인덱스

    // 클라이언트 이름 수신
    str_len = read(clnt_sock, msg, sizeof(msg) - 1);
    msg[str_len] = '\0';

    pthread_mutex_lock(&mutx);
    // 클라이언트 인덱스 찾기
    while (clnt_names[idx][0] != '\0') idx++;
    strcpy(clnt_names[idx], msg);
    clnt_rooms[idx] = room;

    // 첫 번째 방에 클라이언트 추가
    clnt_socks[room][clnt_cnt[room]++] = clnt_sock;
    pthread_mutex_unlock(&mutx);

    while ((str_len = read(clnt_sock, msg, sizeof(msg))) != 0) {
        if (strncmp(msg, "/game", 5) == 0) {
            handle_random_game(clnt_sock, room); // 랜덤 게임 처리
        } else if (strncmp(msg, "/move", 5) == 0) {
            int new_room = atoi(msg + 6) - 1;  // "/move N" 명령어 처리
            change_room(clnt_sock, new_room); // 방 이동 처리
            room = new_room;
        } else {
            send_msg(msg, str_len, room); // 메시지 전송
        }
    }

    // 연결이 끊긴 클라이언트 제거
    pthread_mutex_lock(&mutx);
    for (i = 0; i < clnt_cnt[room]; i++)
    {
        if (clnt_sock == clnt_socks[room][i])
        {
            while (i++ < clnt_cnt[room] - 1) {
                clnt_socks[room][i] = clnt_socks[room][i + 1];
            }
            break;
        }
    }
    clnt_cnt[room]--;
    clnt_names[idx][0] = '\0';
    clnt_rooms[idx] = -1;
    pthread_mutex_unlock(&mutx);
    close(clnt_sock);
    return NULL;
}

// 특정 방의 모든 클라이언트에게 메시지 전송
void send_msg(char* msg, int len, int room)
{
    int i;
    pthread_mutex_lock(&mutx);
    for (i = 0; i < clnt_cnt[room]; i++)
        write(clnt_socks[room][i], msg, len);
    pthread_mutex_unlock(&mutx);
}

// 오류 발생 시 프로그램 종료
void error_handling(char *msg)
{
    fputs(msg, stderr);
    fputc('\n', stderr);
    exit(1);
}

// 서버 상태 반환
char* serverState(int count)
{
    char* stateMsg = malloc(sizeof(char) * 20);
    strcpy(stateMsg, "None");

    if (count < 5)
        strcpy(stateMsg, "Good");
    else
        strcpy(stateMsg, "Bad");

    return stateMsg;
}

// 서버 시작 시 메뉴 출력
void menu(char port[])
{
    system("clear");
    printf(" <<<< Chat server >>>>\n");
    printf(" Server port    : %s\n", port);
    printf(" Server state   : %s\n", serverState(clnt_cnt[0]));
    printf(" Max Client : %d\n", MAX_CLNT);
    printf(" <<<<          Log         >>>>\n\n");
}

// 랜덤 게임 처리 함수
void handle_random_game(int clnt_sock, int room)
{
    int max_rand = 0;
    int winner_sock = -1;
    char msg[BUF_SIZE];
    char winner_name[NAME_SIZE];

    for (int i = 0; i < clnt_cnt[room]; i++) {
        int rand_num = rand() % 100 + 1; // 1부터 100까지의 랜덤 숫자 생성
        if (rand_num > max_rand) {
            max_rand = rand_num;
            winner_sock = clnt_socks[room][i];
            strcpy(winner_name, clnt_names[i]);  // 승자 이름 저장
        }
        sprintf(msg, "%s's number: %d\n", clnt_names[i], rand_num);
        send_msg(msg, strlen(msg), room);
    }
    if (winner_sock != -1) {
        sprintf(msg, "Winner is %s with number %d!\n", winner_name, max_rand);
        send_msg(msg, strlen(msg), room);
    }
}

// 클라이언트의 방을 변경하는 함수
void change_room(int clnt_sock, int new_room)
{
    int i;
    char msg[BUF_SIZE];
    int old_room = -1;

    pthread_mutex_lock(&mutx);
    for (i = 0; i < MAX_CLNT; i++) {
        if (clnt_sock == clnt_socks[clnt_rooms[i]][i]) {
            old_room = clnt_rooms[i];
            break;
        }
    }
    if (old_room == -1 || new_room < 0 || new_room >= MAX_ROOMS) {
        pthread_mutex_unlock(&mutx);
        return;
    }
    // 현재 방에서 클라이언트 제거
    for (i = 0; i < clnt_cnt[old_room]; i++) {
        if (clnt_sock == clnt_socks[old_room][i]) {
            while (i < clnt_cnt[old_room] - 1) {
                clnt_socks[old_room][i] = clnt_socks[old_room][i + 1];
                i++;
            }
            clnt_cnt[old_room]--;
            break;
        }
    }
    // 새 방에 클라이언트 추가
    clnt_socks[new_room][clnt_cnt[new_room]++] = clnt_sock;
    clnt_rooms[i] = new_room;
    pthread_mutex_unlock(&mutx);

    sprintf(msg, "You moved to room %d\n", new_room + 1);
    write(clnt_sock, msg, strlen(msg));
}
