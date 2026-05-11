/*******************************************************************************
 * @file    multiprocess_server.c
 * @brief   Mô tả ngắn gọn về chức năng của file
 * @date    2026-05-05 08:30
 *******************************************************************************/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <netdb.h>
#include <signal.h>
#include <sys/wait.h>
#include <stdbool.h>

bool check_login(const char *user, const char *pass) {
    FILE *f = fopen("login.txt", "r");              
    if (f == NULL) {
        perror("Failed to open login.txt");
        return false;
    }
    char u[50], p[50];
    bool found = false;
    while (fscanf(f, "%s %s", u, p) != EOF) {
        if (strcmp(user, u) == 0 && strcmp(pass, p) == 0) {
            found = true;
            break;
        }
    }
    fclose(f);
    return found;
}

void handle_client(int client)
{
    char buf[1024];

    char user[100];
    char pass[100];

    // ===== USER =====

    send(client, "Username: ", 10, 0);

    int ret = recv(client,
            user,
            sizeof(user) - 1,
            0);

    if (ret <= 0)
    {
        close(client);
        return;
    }

    user[ret] = '\0';

    user[strcspn(user, "\r\n")] = 0;

    // ===== PASSWORD =====

    send(client, "Password: ", 10, 0);

    ret = recv(client,
            pass,
            sizeof(pass) - 1,
            0);

    if (ret <= 0)
    {
        close(client);
        return;
    }

    pass[ret] = '\0';

    pass[strcspn(pass, "\r\n")] = 0;

    // ===== CHECK LOGIN =====

    if (!check_login(user, pass))
    {
        char *fail =
            "Dang nhap that bai!\n";

        send(client,
            fail,
            strlen(fail),
            0);

        close(client);
        return;
    }

    if (!user || !pass ||
        !check_login(user, pass))
    {
        char *fail = "Dang nhap that bai!\n";

        send(client, fail, strlen(fail), 0);

        close(client);
        return;
    }

    char *ok = "Dang nhap thanh cong!\n";

    send(client, ok, strlen(ok), 0);

    while (1)
    {
        char *prompt = "Nhap lenh (exit de thoat): ";

        send(client,prompt, strlen(prompt), 0);

        ret = recv(client, buf, sizeof(buf) - 1, 0);

        if (ret <= 0)
            break;

        buf[ret] = '\0';

        // xoa newline
        buf[strcspn(buf, "\r\n")] = 0;

        // exit
        if (strcmp(buf, "exit") == 0)
        {
            break;
        }

        // tao command
        char cmd[1200];

        snprintf(cmd,sizeof(cmd),"%s > out.txt 2>&1", buf);

        system(cmd);

        FILE *f = fopen("out.txt", "r");

        if (f == NULL)
        {
            char *err = "Khong mo duoc out.txt\n";

            send(client, err, strlen(err), 0);

            continue;
        }

        char output[4096];

        int n = fread(output, 1, sizeof(output) - 1, f);

        output[n] = '\0';

        fclose(f);

        send(client, output, strlen(output),0);
    }

    close(client);
}

void sigchld_handler(int signum) {
    (void)signum; // Unused parameter
    while (waitpid(-1, NULL, WNOHANG) > 0);
}


int main() {
    int listener = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (listener == -1) {
        perror("socket() failed");
        return 1;
    }
    
    if (setsockopt(listener, SOL_SOCKET, SO_REUSEADDR, &(int){1}, sizeof(int))) {
        perror("setsockopt() failed");
        close(listener);
        return 1;
    }
    
    struct sockaddr_in addr = {0};
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_ANY);
    addr.sin_port = htons(8080);
    
    if (bind(listener, (struct sockaddr *)&addr, sizeof(addr))) {
        perror("bind() failed");
        close(listener);
        return 1;
    }
    
    if (listen(listener, 5)) {
        perror("listen() failed");
        close(listener);
        return 1;
    }

    printf("Server is listening on port 8080...\n");
    signal(SIGCHLD, sigchld_handler);

    while (1) {
        struct sockaddr_in client_addr;
        socklen_t client_len = sizeof(client_addr);
        int client = accept(listener, (struct sockaddr *)&client_addr, &client_len);
        if (client == -1) {
            perror("accept() failed");
            continue;
        }
        printf("Client connected: %s:%d\n", inet_ntoa(client_addr.sin_addr), ntohs(client_addr.sin_port));

        if (fork() == 0) {
            close(listener);
            handle_client(client);
            exit(0);
        } else {
            close(client); 
        }
    }
    close(listener);
    return 0;
}