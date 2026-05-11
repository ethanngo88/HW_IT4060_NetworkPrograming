#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <sys/select.h>

#define PORT 8080
#define MAX_CLIENTS 20
#define MAX_BUFFER 1024

// Hàm mã hóa chuỗi
void encrypt(char *str) {
    for (int i = 0; str[i] != '\0'; i++) {
        if (str[i] >= 'a' && str[i] <= 'z') {
            str[i] = (str[i] - 'a' + 1) % 26 + 'a';
        } else if (str[i] >= 'A' && str[i] <= 'Z') {
            str[i] = (str[i] - 'A' + 1) % 26 + 'A';
        } else if (str[i] >= '0' && str[i] <= '9') {
            int num = str[i] - '0';
            str[i] = (9 - num) + '0';
        }
    }
}

int main() {
    int server_fd, new_socket, client_sockets[MAX_CLIENTS];
    struct sockaddr_in server_addr, client_addr;
    int addrlen = sizeof(client_addr);

    fd_set readfds;
    int max_sd, sd, activity;
    char buffer[MAX_BUFFER];

    // Khởi tạo mảng client_sockets
    for (int i = 0; i < MAX_CLIENTS; i++) {
        client_sockets[i] = 0;
    }

    // Tạo socket
    server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (server_fd == 0) {
        perror("socket failed");
        exit(EXIT_FAILURE);
    }

    // Cho phép dùng lại địa chỉ
    int opt = 1;
    if (setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) < 0) {
        perror("setsockopt failed");
        exit(EXIT_FAILURE);
    }

    // Cấu hình địa chỉ server
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY;
    server_addr.sin_port = htons(PORT);

    // Bind
    if (bind(server_fd,
             (struct sockaddr *)&server_addr,
             sizeof(server_addr)) < 0) {
        perror("bind failed");
        exit(EXIT_FAILURE);
    }

    // Listen
    if (listen(server_fd, 3) < 0) {
        perror("listen failed");
        exit(EXIT_FAILURE);
    }

    printf("Server is listening on port %d\n", PORT);

    while (1) {

        // Khởi tạo lại tập socket
        FD_ZERO(&readfds);
        FD_SET(server_fd, &readfds);
        max_sd = server_fd;

        // Thêm các client socket vào readfds
        for (int i = 0; i < MAX_CLIENTS; i++) {
            sd = client_sockets[i];

            if (sd > 0)
                FD_SET(sd, &readfds);

            if (sd > max_sd)
                max_sd = sd;
        }

        // Chờ hoạt động
        activity = select(max_sd + 1, &readfds, NULL, NULL, NULL);

        if (activity < 0) {
            perror("select error");
            exit(EXIT_FAILURE);
        }

        // Nếu có kết nối mới
        if (FD_ISSET(server_fd, &readfds)) {

            new_socket = accept(server_fd, (struct sockaddr *)&client_addr, (socklen_t *)&addrlen);

            if (new_socket < 0) {
                perror("accept failed");
                exit(EXIT_FAILURE);
            }

            int client_count = 1; 

            for (int i = 0; i < MAX_CLIENTS; i++) {
                if (client_sockets[i] > 0) {
                    client_count++;
                }
            }

            // Gửi lời chào
            char welcome_msg[MAX_BUFFER];
            snprintf(welcome_msg,
                     sizeof(welcome_msg),
                     "Xin chao. Hien co %d clients dang ket noi.\n",
                     client_count);

            send(new_socket, welcome_msg, strlen(welcome_msg), 0);

            // Lưu socket mới vào mảng
            for (int i = 0; i < MAX_CLIENTS; i++) {
                if (client_sockets[i] == 0) {
                    client_sockets[i] = new_socket;
                    printf("Client %d da ket noi \n", new_socket);
                    break;
                }
            }
        }


        for (int i = 0; i < MAX_CLIENTS; i++) {
            sd = client_sockets[i];

            if (sd > 0 && FD_ISSET(sd, &readfds)) {
                memset(buffer, 0, MAX_BUFFER);
                int valread = read(sd, buffer, MAX_BUFFER);
                if (valread <= 0) {
                    printf("Client %d ngat ket noi\n", sd);
                    close(sd);
                    client_sockets[i] = 0;
                }
                else {
                    buffer[valread] = '\0';
                    buffer[strcspn(buffer, "\r\n")] = '\0';
                    printf("Du lieu nhan tu client %d: %s\n", sd, buffer);
                    if (strcmp(buffer, "exit") == 0) {
                        char goodbye_msg[] = "Tam biet!\n";
                        send(sd, goodbye_msg, strlen(goodbye_msg), 0);
                        close(sd);
                        client_sockets[i] = 0;
                        printf("Dong ket noi voi client %d\n", sd);
                    }
                    else {
                        encrypt(buffer);
                        send(sd, buffer,strlen(buffer),0);
                    }
                }
            }
        }
    }

    close(server_fd);
    return 0;
}