// 채팅 프로그램 서버

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

void * handle_clnt(void *arg);
void send_msg(char *msg, int len, int room);
void error_handling(char *msg);
char* serverState(int count);
void menu(char port[]);
void handle_random_game(int clnt_sock, int room);
void change_room(int clnt_sock, int new_room);

/****************************/

int clnt_cnt[MAX_ROOMS] = {0}; // 각 방의 클라이언트 수
int clnt_socks[MAX_ROOMS][MAX_CLNT];
char clnt_names[MAX_CLNT][NAME_SIZE];  // 클라이언트 이름 저장 배열
int clnt_rooms[MAX_CLNT];  // 클라이언트 방 번호 저장 배열
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
        timer = time(NULL); // get time
        t = localtime(&timer);
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

    // 첫 번째 방에 추가
    clnt_socks[room][clnt_cnt[room]++] = clnt_sock;
    pthread_mutex_unlock(&mutx);

    while ((str_len = read(clnt_sock, msg, sizeof(msg))) != 0) {
        if (strncmp(msg, "/game", 5) == 0) {
            handle_random_game(clnt_sock, room);
        } else if (strncmp(msg, "/move", 5) == 0) {
            int new_room = atoi(msg + 6) - 1;  // "/move N" 명령어 처리
            change_room(clnt_sock, new_room);
            room = new_room;
        } else {
            send_msg(msg, str_len, room);
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

void send_msg(char* msg, int len, int room)
{
    int i;
    pthread_mutex_lock(&mutx);
    for (i = 0; i < clnt_cnt[room]; i++)
        write(clnt_socks[room][i], msg, len);
    pthread_mutex_unlock(&mutx);
}

void error_handling(char *msg)
{
    fputs(msg, stderr);
    fputc('\n', stderr);
    exit(1);
}

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

void menu(char port[])
{
    system("clear");
    printf(" <<<< Chat server >>>>\n");
    printf(" Server port    : %s\n", port);
    printf(" Server state   : %s\n", serverState(clnt_cnt[0]));
    printf(" Max Client : %d\n", MAX_CLNT);
    printf(" <<<<          Log         >>>>\n\n");
}

void handle_random_game(int clnt_sock, int room)
{
    int max_rand = 0;
    int winner_sock = -1;
    char msg[BUF_SIZE];
    char winner_name[NAME_SIZE];

    for (int i = 0; i < clnt_cnt[room]; i++) {
        int rand_num = rand() % 100 + 1; // 미니게임용 난수 1~100
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
    // 현재 방에서 제거
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
    // 새 방에 추가
    clnt_socks[new_room][clnt_cnt[new_room]++] = clnt_sock;
    clnt_rooms[i] = new_room;
    pthread_mutex_unlock(&mutx);

    sprintf(msg, "채팅룸 %d로 변경.\n", new_room + 1);
    write(clnt_sock, msg, strlen(msg));
}
