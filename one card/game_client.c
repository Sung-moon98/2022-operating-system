#include <stdio.h>
#include <sys/socket.h>
#include <stdlib.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <string.h>

struct card {
	int value; // ī�� ���� (1~13)
	char suit; // ī�� ����
};
struct socket_msg{
        char text[1024];
        int flag;
};
void main(){
    struct sockaddr_in s_addr;
    int sock_fd;
    char buffer[1024] = {0};
    struct socket_msg msg;
    int check;

    // ���� ����
    sock_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (sock_fd <= 0){
        perror("socket failed: ");
        exit(1);
    }

    // ���� ����
    memset(&s_addr, '0', sizeof(s_addr));
    s_addr.sin_family = AF_INET;
    s_addr.sin_port = htons(8080);    
    check = inet_pton(AF_INET, "127.0.0.1", &s_addr.sin_addr);
    if (check<=0){
        perror("inet_pton failed: ");
        exit(1);
    }
    check = connect(sock_fd, (struct sockaddr *) &s_addr, sizeof(s_addr));
    if(check < 0){
        perror("connect failed: ");
        exit(1);
    }

    while(1){
        recv(sock_fd, (struct socket_msg*)&msg, sizeof(msg), 0);

        if (msg.flag == 0){ // �ȳ����� ���
            printf("%s", msg.text);
        }
        else if (msg.flag == 1){ // �Է¿��
            printf("%s", msg.text);
            scanf("%d", &msg.flag);
            send(sock_fd, (struct socket_msg*)&msg, sizeof(msg), 0);
        }
        else if (msg.flag == -99){ // ����
            printf("%s", msg.text);
            exit(0);
        }
    }
}