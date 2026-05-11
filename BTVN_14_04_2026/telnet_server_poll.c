#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <poll.h>

#define PORT 8080
#define MAX_CLIENTS 100
#define BUF_SIZE 1024

int check_login(char *user, char *pass) {
    FILE *f = fopen("login.txt", "r");
    if (f == NULL) return 0;

    char u[50], p[50];
    while (fscanf(f, "%49s %49s", u, p) != EOF) {
        if (strcmp(user, u) == 0 && strcmp(pass, p) == 0) {
            fclose(f);
            return 1;
        }
    }

    fclose(f);
    return 0;
}

int main() {
    int listener = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (listener < 0) return 1;

    int opt = 1;
    setsockopt(listener, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));

    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_ANY);
    addr.sin_port = htons(PORT);

    bind(listener, (struct sockaddr *)&addr, sizeof(addr));
    listen(listener, 5);

    printf("Telnet Server is listening on port %d...\n", PORT);

    struct pollfd fds[MAX_CLIENTS];
    int logged_in[MAX_CLIENTS];
    char buf[BUF_SIZE];

    for (int i = 0; i < MAX_CLIENTS; i++) {
        fds[i].fd = -1;
        fds[i].events = POLLIN;
        logged_in[i] = 0;
    }

    fds[0].fd = listener;
    fds[0].events = POLLIN;

    while (1) {
        int activity = poll(fds, MAX_CLIENTS, -1);
        if (activity < 0) continue;

        for (int i = 0; i < MAX_CLIENTS; i++) {
            if (fds[i].fd == -1) continue;
            if (!(fds[i].revents & POLLIN)) continue;

            // Có client mới kết nối
            if (fds[i].fd == listener) {
                int client = accept(listener, NULL, NULL);
                if (client < 0) continue;

                for (int j = 1; j < MAX_CLIENTS; j++) {
                    if (fds[j].fd == -1) {
                        fds[j].fd = client;
                        fds[j].events = POLLIN;
                        logged_in[j] = 0;

                        send(client,
                             "Hay nhap user pass de dang nhap: ",
                             33, 0);
                        break;
                    }
                }
            }

            // Client gửi dữ liệu
            else {
                int sock = fds[i].fd;
                int ret = recv(sock, buf, sizeof(buf) - 1, 0);

                if (ret <= 0) {
                    close(sock);
                    fds[i].fd = -1;
                    logged_in[i] = 0;
                }
                else {
                    buf[ret] = 0;

                    if (ret > 0 && buf[ret - 1] == '\n')
                        buf[ret - 1] = 0;

                    if (ret > 1 && buf[ret - 2] == '\r')
                        buf[ret - 2] = 0;

                    // Chưa đăng nhập
                    if (!logged_in[i]) {
                        char user[50], pass[50];

                        if (sscanf(buf, "%49s %49s", user, pass) == 2 &&
                            check_login(user, pass)) {

                            logged_in[i] = 1;

                            send(sock,
                                 "Dang nhap thanh cong! Nhap lenh:\n",
                                 33, 0);
                        }
                        else {
                            send(sock,
                                 "Loi dang nhap! Nhap lai user pass: ",
                                 35, 0);
                        }
                    }

                    // Đã đăng nhập -> chạy lệnh
                    else {
                        char cmd[1100];
                        snprintf(cmd, sizeof(cmd),
                                 "%s > out.txt 2>&1", buf);

                        system(cmd);

                        FILE *f = fopen("out.txt", "rb");
                        if (f) {
                            while (1) {
                                int n = fread(buf, 1,
                                              sizeof(buf), f);
                                if (n <= 0) break;
                                send(sock, buf, n, 0);
                            }
                            fclose(f);
                        }
                    }
                }
            }
        }
    }

    return 0;
}