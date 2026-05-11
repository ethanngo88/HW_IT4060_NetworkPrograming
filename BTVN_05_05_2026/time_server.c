
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
#include <time.h>


void handle_client(int client)
{
   char buf[1024];
   while (1){
        int ret = recv(client, buf, sizeof(buf) - 1, 0);
        if (ret <= 0)
        {
            break;
        }
        buf[ret] = '\0';
        buf[strcspn(buf, "\r\n")] = 0;
        printf("Client %d sent: %s\n", client, buf);
        char *cmd = strtok(buf, " ");
        char *format = strtok(NULL, " ");

        if (!cmd || !format || strcmp(cmd, "GET_TIME") != 0)
        {
            char *err = "Invalid command format. Use: GET_TIME <format>\n";
            send(client, err, strlen(err), 0);
            continue;
        }

        time_t t = time(NULL);
        struct tm *tm_info = localtime(&t);
        char result[256];

        if (strcmp(format, "dd/mm/yyyy") == 0)
        {
            strftime(result, sizeof(result), "%d/%m/%Y", tm_info);
        }
        else if (strcmp(format, "dd/mm/yy") == 0)
        {
            strftime(result, sizeof(result),"%d/%m/%y", tm_info);
        }
        else if (strcmp(format, "mm/dd/yyyy") == 0)
        {
            strftime(result, sizeof(result), "%m/%d/%Y", tm_info);
        }
        else if (strcmp(format, "mm/dd/yy") == 0)
        {
            strftime(result, sizeof(result), "%m/%d/%y", tm_info);
        }
        else
        {
            char *err = "Unsupported format\n";
            send(client, err, strlen(err), 0);
            continue;
        }
        strcat(result, "\n");
        send(client, result, strlen(result), 0);

   }
    close(client);
}

void sigchld_handler(int signum) {
    (void)signum; 
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
        int client = accept(listener, NULL, NULL);
        if (client == -1) {
            perror("accept() failed");
            continue;
        }

        printf("Accepted client %d\n", client);

        pid_t pid = fork();

        if (pid == 0) {
            close(listener);
            handle_client(client);
            exit(0);
        }
        else if (pid > 0) {
            close(client);
        }
        else {
            perror("fork() failed");
            close(client);
        }
    }
    close(listener);
    return 0;
}