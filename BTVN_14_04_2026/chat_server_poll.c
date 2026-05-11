#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <poll.h>
#include <time.h>

#define PORT 8080
#define MAX_NAME 50
#define BUF_SIZE 1024
#define MAX_CLIENTS 100

int main() {
    int listener = socket(AF_INET, SOCK_STREAM, 0);
    if (listener < 0) return 1;

    int opt = 1;
    setsockopt(listener, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_ANY);
    addr.sin_port = htons(PORT);

    if (bind(listener, (struct sockaddr*)&addr, sizeof(addr)) < 0) return 1;
    if (listen(listener, 10) < 0) return 1;

    struct pollfd fds[MAX_CLIENTS];
    char client_ids[MAX_CLIENTS][MAX_NAME];
    char buf[BUF_SIZE];

    for (int i = 0; i < MAX_CLIENTS; i++) {
        fds[i].fd = -1;
        fds[i].events = POLLIN;
        client_ids[i][0] = 0;
    }

    fds[0].fd = listener;
    fds[0].events = POLLIN;

    printf("Chat server listening on port %d...\n", PORT);

    while (1) {
        int activity = poll(fds, MAX_CLIENTS, -1);
        if (activity < 0) continue;

        for (int i = 0; i < MAX_CLIENTS; i++) {
            if (fds[i].fd == -1) continue;
            if (!(fds[i].revents & POLLIN)) continue;

            // Socket listener
            if (fds[i].fd == listener) {
                int client = accept(listener, NULL, NULL);
                if (client < 0) continue;

                for (int j = 1; j < MAX_CLIENTS; j++) {
                    if (fds[j].fd == -1) {
                        fds[j].fd = client;
                        fds[j].events = POLLIN;
                        char *msg = "Nhap theo cu phap: client_id: NguyenVanA\n";
                        send(client, msg, strlen(msg), 0);
                        break;
                    }
                }
            }
            // Client gửi dữ liệu
            else {
                int sock = fds[i].fd;
                int ret = recv(sock, buf, sizeof(buf) - 1, 0);

                if (ret <= 0) {
                    if (client_ids[i][0]) {
                        char msg[128];
                        snprintf(msg, sizeof(msg),
                                 "%s da roi phong chat.\n",
                                 client_ids[i]);

                        for (int j = 1; j < MAX_CLIENTS; j++) {
                            if (fds[j].fd != -1 &&
                                j != i &&
                                client_ids[j][0]) {
                                send(fds[j].fd, msg, strlen(msg), 0);
                            }
                        }
                    }

                    close(sock);
                    fds[i].fd = -1;
                    client_ids[i][0] = 0;
                }
                else {
                    buf[ret] = 0;
                    buf[strcspn(buf, "\r\n")] = 0;

                    // Chưa đăng nhập
                    if (client_ids[i][0] == 0) {
                        char key[MAX_NAME], name[MAX_NAME];

                        if (sscanf(buf, "%49[^:]: %49s", key, name) == 2 &&
                            strcmp(key, "client_id") == 0) {

                            strncpy(client_ids[i], name, MAX_NAME - 1);

                            send(sock, "Dang nhap thanh cong!\n", 22, 0);

                            char joinmsg[128];
                            snprintf(joinmsg, sizeof(joinmsg),
                                     "%s da tham gia phong chat.\n",
                                     client_ids[i]);

                            for (int j = 1; j < MAX_CLIENTS; j++) {
                                if (fds[j].fd != -1 &&
                                    j != i &&
                                    client_ids[j][0]) {
                                    send(fds[j].fd, joinmsg,
                                         strlen(joinmsg), 0);
                                }
                            }
                        }
                        else {
                            send(sock,
                                 "Sai cu phap! Dung: client_id: NguyenVanA\n",
                                 42, 0);
                        }
                    }
                    // Đã đăng nhập -> chat
                    else {
                        time_t now = time(NULL);
                        struct tm *t = localtime(&now);

                        char timestr[64];
                        strftime(timestr, sizeof(timestr),
                                 "%Y/%m/%d %I:%M:%S%p", t);

                        char out[2048];
                        snprintf(out, sizeof(out),
                                 "%s %s: %s\n",
                                 timestr, client_ids[i], buf);

                        for (int j = 1; j < MAX_CLIENTS; j++) {
                            if (fds[j].fd != -1 &&
                                j != i &&
                                client_ids[j][0]) {
                                send(fds[j].fd, out, strlen(out), 0);
                            }
                        }
                    }
                }
            }
        }
    }

    return 0;
}