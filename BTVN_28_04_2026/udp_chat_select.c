#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <sys/select.h>

#define MAX_CLIENTS 200
#define MAX_BUFFER 1024


int main(int argc, char *argv[]) {
    if(argc != 4){
        fprintf(stderr, "Usage: %s <port> <remote_ip> <remote_port>\n", argv[0]);
        exit(EXIT_FAILURE);
    }

    int port = atoi(argv[1]);
    char *remote_ip = argv[2];
    int remote_port = atoi(argv[3]);

    int sockfd;
    struct sockaddr_in server_addr, remote_addr;
    socklen_t addr_len = sizeof(remote_addr);

    fd_set readfds;
    char buffer[MAX_BUFFER];


    // Tạo socket UDP
    if ((sockfd = socket(AF_INET, SOCK_DGRAM, 0)) < 0) {
        perror("socket failed");
        exit(EXIT_FAILURE);
    }

    // Cấu hình địa chỉ server
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY;
    server_addr.sin_port = htons(port);

    // Cấu hình địa chỉ remote
    memset(&remote_addr, 0, sizeof(remote_addr));
    remote_addr.sin_family = AF_INET;
    remote_addr.sin_port = htons(remote_port);
    if (inet_pton(AF_INET, remote_ip, &remote_addr.sin_addr) <= 0) {
        perror("inet_pton failed");
        exit(EXIT_FAILURE);
    }

    // Bind socket
    if (bind(sockfd, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
        perror("bind failed");
        exit(EXIT_FAILURE);
    }

    printf("UDP chat server is running on port %d\n", port);
    printf("Chatting with remote server at %s:%d\n", remote_ip, remote_port);
    printf("Type 'exit' to quit.\n");

    while (1){
        FD_ZERO(&readfds);
        FD_SET(sockfd, &readfds);
        FD_SET(STDIN_FILENO, &readfds);
        int maxfd = sockfd > STDIN_FILENO ? sockfd : STDIN_FILENO;

        int activity = select(maxfd + 1, &readfds, NULL, NULL, NULL);

        if (activity < 0) {
            perror("select error");
            exit(EXIT_FAILURE);
        }

        // Gửi tin nhắn từ stdin
        if (FD_ISSET(STDIN_FILENO, &readfds)) {
            fgets(buffer, MAX_BUFFER, stdin);
            buffer[strcspn(buffer, "\n")] = 0; // Xóa newline
            if (strcmp(buffer, "exit") == 0) {
                printf("Exiting chat...\n");
                break;
            }
            sendto(sockfd, buffer, strlen(buffer), 0, (struct sockaddr *)&remote_addr, sizeof(remote_addr));
        }

        // Nhận tin nhắn từ remote
        if (FD_ISSET(sockfd, &readfds)) {
            memset(buffer, 0, MAX_BUFFER);
            int len = recvfrom(sockfd, buffer, MAX_BUFFER, 0, (struct sockaddr *)&remote_addr, &addr_len);
            if (len > 0) {
                buffer[len] = '\0';
                printf("Received from %s:%d: %s\n", inet_ntoa(remote_addr.sin_addr), ntohs(remote_addr.sin_port), buffer);
                if (strcmp(buffer, "exit") == 0) {
                    printf("Remote server has exited the chat. Exiting...\n");
                    break;
                }
            }
        }
    }   
    close(sockfd);
    return 0;
}