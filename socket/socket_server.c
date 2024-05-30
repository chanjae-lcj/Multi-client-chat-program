#include <unistd.h>
#include <stdio.h>
#include <sys/socket.h>
#include <stdlib.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <string.h>

void child_proc(int sock);
struct sending_packet receive_sock(int sock);
void send_sock(int sock, struct sending_packet pck);

 struct sending_packet{
    char sender[1024];
    char receiver[1024];
    char msg[1024];
};

void main(){
    int s_sock_fd, new_sock_fd;
    struct sockaddr_in s_address, c_address;
    int addrlen = sizeof(s_address);
    int check;

    // 서버 소켓 선언
    s_sock_fd = socket(AF_INET, SOCK_STREAM, 0);// AF_INET: IPv4 인터넷 프로토콜, SOCK_STREAM: TCP/IP
    if (s_sock_fd == -1){
        perror("socket failed: ");
        exit(1);
    }
    
    // 서버 소켓 셋팅
    memset(&s_address, '0', addrlen); // s_address의 메모리 공간을 0으로 셋팅 / memset(메모리 공간 시작점, 채우고자 하는 값, 메모리 길이)
    s_address.sin_family = AF_INET; // IPv4 인터넷 프로토콜
    s_address.sin_addr.s_addr = inet_addr("127.0.0.1"); // 내부 IP 주소
    s_address.sin_port = htons(8080); // 포트번호 지정
    check = bind(s_sock_fd, (struct sockaddr *)&s_address, sizeof(s_address));
    if(check == -1){
        perror("bind error: ");
        exit(1);
    }

    // 아래부터 소켓 통신 시작
    while(1){
        // 클라이언트의 접속을 기다림 (한 클라이언트가 서버 소켓을 사용하고 있으면, 다른 클라이언트는 대기열에서 기다리게 됨)
        check = listen(s_sock_fd, 16);
        if(check==-1){
            perror("listen failed: ");
            exit(1);
        }

        // 클라이언트의 접속을 허가함 -> 접속에 성공한 클라이언트와의 통신을 위해 새로운 소켓을 생성함
        new_sock_fd = accept(s_sock_fd, (struct sockaddr *) &c_address, (socklen_t*)&addrlen);
        if(new_sock_fd<0){
            perror("accept failed: ");
            exit(1);
        } 

        //접속에 성공한 클라이언트와의 통신은 자식 프로세스를 통해서 수행함
        if(fork()>0){ // 자식 프로세스
            child_proc(new_sock_fd);
        }
        else{ // 부모 프로세스
            // 부모 프로세스는 새로운 소켓을 유지할 필요 없음.
            close(new_sock_fd);
        }
    }
}

void child_proc(int sock){
    struct sending_packet pck;
    int flag = 0;

    while(1){
        pck = receive_sock(sock); // 소켓을 통해 데이터 수신
        printf("%s: %s\n", pck.sender, pck.msg); // 수신받은 데이터 출력
        if (strcmp(pck.msg,"quit")==0){
            flag = -1;
        }
        
        // packet 정보 수정
        sprintf(pck.msg, "Message received!");
        sprintf(pck.sender, "Server");
        sprintf(pck.receiver, "Client");

        send_sock(sock, pck); // 데이터 송신
        if (flag == -1){
            break;
        }
    }
    shutdown(sock, SHUT_WR); // 소켓 Close
    exit(0);
}

struct sending_packet receive_sock(int sock){
    struct sending_packet pck;
    recv(sock, (struct sending_packet*)&pck, sizeof(pck),0);
    return pck;
}

void send_sock(int sock, struct sending_packet pck){
    send(sock, (struct sending_packet*)&pck, sizeof(pck), 0);
}