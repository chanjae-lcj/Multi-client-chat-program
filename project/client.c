// 채팅 프로그램 클라이언트

#include<stdio.h>
#include<stdlib.h>
#include<unistd.h>
#include<string.h>
#include<arpa/inet.h>
#include<pthread.h>
#include<time.h>

#define BUF_SIZE 100
#define NORMAL_SIZE 20

void* send_msg(void* arg);
void* recv_msg(void* arg);
void error_handling(char* msg);
void menu();
void menuOptions();
void updateMenu();

char msg[BUF_SIZE];
char name[NORMAL_SIZE] = "[DEFAULT]";
char serv_time[NORMAL_SIZE];           // server time
char serv_port[NORMAL_SIZE];           // server port number
char clnt_ip[NORMAL_SIZE];             // client ip address

int mode = 1; // 1: chat mode, 0: menu mode
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

    /** local time **/
    struct tm *t;
    time_t timer = time(NULL);
    t = localtime(&timer);
    sprintf(serv_time, "%d-%d-%d %d:%d", t->tm_year + 1900, t->tm_mon + 1, t->tm_mday, t->tm_hour, t->tm_min);

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

    /** call menu **/
    menu();

    // 클라이언트 이름 전송
    write(sock, name, strlen(name));

    pthread_create(&snd_thread, NULL, send_msg, (void*)&sock);
    pthread_create(&rcv_thread, NULL, recv_msg, (void*)&sock);
    pthread_join(snd_thread, &thread_return);
    pthread_join(rcv_thread, &thread_return);
    close(sock);
    return 0;
}

void* send_msg(void* arg)
{
    int sock = *((int*)arg);
    char name_msg[NORMAL_SIZE + BUF_SIZE];
    char myInfo[BUF_SIZE];

    /** send join message **/
    printf(" >> join the chat !! \n");
    sprintf(myInfo, "%s's join. IP_%s\n", name, clnt_ip);
    write(sock, myInfo, strlen(myInfo));

    while (1)
    {
        if (mode == 0) {
            menuOptions();
        } else {
            fgets(msg, BUF_SIZE, stdin);

            if (!strcmp(msg, "/q\n") || !strcmp(msg, "/Q\n"))
            {
                close(sock);
                exit(0);
            }

            if (!strcmp(msg, "/m\n") || !strcmp(msg, "/M\n")) {
                mode = 0; // switch to menu mode
                continue;
            }

            // send message
            sprintf(name_msg, "%s %s", name, msg);
            write(sock, name_msg, strlen(name_msg));
        }
    }
    return NULL;
}

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

        if (strstr(name_msg, "채팅룸을 옮겼습니다.") != NULL) {
            sscanf(name_msg, "--- %d채팅룸으로 이동했습니다. ---", &current_room);
            updateMenu();
        }
    }
    return NULL;
}

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

void menuOptions()
{
    int option;
    printf("mode를 선택하세요.: ");
    scanf("%d", &option);
    getchar(); // to consume the newline character after entering the option
    switch (option)
    {
        case 1: {
            char new_name[NORMAL_SIZE];
            printf("Enter new name: ");
            fgets(new_name, NORMAL_SIZE, stdin);
            new_name[strcspn(new_name, "\n")] = 0; // remove newline character

            char name_change_msg[BUF_SIZE];
            sprintf(name_change_msg, "%s has changed their name to [%s]\n", name, new_name);
            sprintf(name, "[%s]", new_name);
            write(sock, name_change_msg, strlen(name_change_msg));
            break;
        }
        case 2:
            menu();
            break;
        case 3:
            sprintf(msg, "/game");
            write(sock, msg, strlen(msg));
            break;
        case 4: {
            int new_room;
            printf("<<< 채팅룸(1~3)을 선택하세요. >>>");
            scanf("%d", &new_room);
            getchar(); // to consume the newline character
            if (new_room < 1 || new_room > 3) {
                printf("!!! 1부터 3까지만 입력하세요. !!!\n");
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
    mode = 1; // switch back to chat mode
}

void error_handling(char* msg)
{
    fputs(msg, stderr);
    fputc('\n', stderr);
    exit(1);
}
