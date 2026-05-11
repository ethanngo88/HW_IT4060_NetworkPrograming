#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <netdb.h>
#include <poll.h>

#define MAX_CLIENTS 1000
#define MAX_TOPICS 50
#define BUF_SIZE 1024

typedef struct {
    char topic[50];
    int subcribes[MAX_CLIENTS];
    int count;
} Topic;

struct pollfd fds[MAX_CLIENTS];
Topic topics[MAX_TOPICS];
int topic_count = 0;

// Tìm kiếm topic, trả về index hoặc -1 nếu không tìm thấy
int find_topic(char *name){
    for(int i = 0; i < topic_count; i++){
        if(strcmp(topics[i].topic, name) == 0){
            return i;
        }
    }
    return -1;
}

// Tạo topic mới và trả về index
int create_topic(char *name){
    strcpy(topics[topic_count].topic, name);
    topics[topic_count].count = 0;
    return topic_count++;
}

// Hàm subcribe, unsubcribe và publish
void subcribe(int fd, char *topic){
    int idx = find_topic(topic);
    if(idx == -1){
        idx = create_topic(topic);
    }

    for(int i = 0; i < topics[idx].count; i++){
        if(topics[idx].subcribes[i] == fd){
            return;
        }
    }

    topics[idx].subcribes[topics[idx].count++] = fd;
    printf("Client %d subscribed to topic %s\n", fd, topic);
}

void unsubcribe(int fd, char *topic){
    int idx = find_topic(topic);
    if(idx == -1){
        return;
    }

    for(int i = 0; i < topics[idx].count; i++){
        if(topics[idx].subcribes[i] == fd){
            for(int j = i; j < topics[idx].count - 1; j++){
                topics[idx].subcribes[j] = topics[idx].subcribes[j + 1];
            }
            topics[idx].count--;
            printf("Client %d unsubscribed from topic %s\n", fd, topic);
            return;
        }
    }
}

void publish(int sender_fd, char *topic, char *msg){
    int idx = find_topic(topic);
    if(idx == -1) return;

    int is_subscribed = 0;
    for(int i = 0; i < topics[idx].count; i++){
        if(topics[idx].subcribes[i] == sender_fd){
            is_subscribed = 1;
            break;
        }
    }

    if (!is_subscribed) {
        char *err_msg = "Error: You are not subscribed to this topic. Please subscribe first.\n";
        send(sender_fd, err_msg, strlen(err_msg), 0);
        return;
    }

    char out[BUF_SIZE];
    for(int i = 0; i < topics[idx].count; i++){
        int fd = topics[idx].subcribes[i];
        snprintf(out, sizeof(out), "Client %d from topic %s: %s\n", sender_fd, topic, msg);
        send(fd, out, strlen(out), 0);
    }
}

void handle_client_message(int fd, char *buf){
    char *cmd = strtok(buf, " ");
    if (cmd == NULL) return;
    
    char *topic = strtok(NULL, " ");
    char *msg = strtok(NULL, "");

    if(strcmp(cmd, "SUB") == 0){
        if (topic != NULL) {
            subcribe(fd, topic);
        }
    } else if(strcmp(cmd, "UNSUB") == 0){
        if (topic != NULL) {
            unsubcribe(fd, topic);
        }
    } else if(strcmp(cmd, "PUB") == 0){
        if (topic != NULL && msg != NULL) {
            publish(fd, topic, msg);
        }
    }
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

    // Cấu hình địa chỉ 
    struct sockaddr_in addr = {0};
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_ANY);
    addr.sin_port = htons(9000);

    // Hàm bind
    if (bind(listener, (struct sockaddr *)&addr, sizeof(addr))) {
        perror("bind() failed");
        close(listener);
        return 1;
    }
    
    // Chờ kết nối từ clients
    if (listen(listener, 5)) {
        perror("listen() failed");
        close(listener);
        return 1;
    }
    
    printf("Server is listening on port 9000...\n");
    
    fds[0].fd = listener;
    fds[0].events = POLLIN;
    int nfds = 1;

    while (1) {
        int poll_count = poll(fds, nfds, -1);
        if (poll_count < 0) {
            perror("poll() failed");
            break;
        }

        for (int i = 0; i < nfds; i++) {
            if (fds[i].revents & POLLIN) {
                if (fds[i].fd == listener) {
                    int newfd = accept(listener, NULL, NULL);
                    if (newfd >= 0) {
                        fds[nfds].fd = newfd;
                        fds[nfds].events = POLLIN;
                        nfds++;
                        printf("New client connected: %d\n", newfd);
                    }
                } else {
                    char buf[BUF_SIZE];
                    int len = recv(fds[i].fd, buf, sizeof(buf), 0);
                    
                    if (len <= 0) {
                        int current_fd = fds[i].fd;
                        close(current_fd);
                        printf("Client %d disconnected\n", current_fd);

                        // Remove from topics
                        for (int t = 0; t < topic_count; t++) {
                            for (int sc = 0; sc < topics[t].count; sc++) {
                                if (topics[t].subcribes[sc] == current_fd) {
                                    for (int j = sc; j < topics[t].count - 1; j++) {
                                        topics[t].subcribes[j] = topics[t].subcribes[j + 1];
                                    }
                                    topics[t].count--;
                                    break;
                                }
                            }
                        }

                        // Remove from the fds array
                        for (int j = i; j < nfds - 1; j++) {
                            fds[j] = fds[j + 1];
                        }
                        nfds--;
                        i--; // Adjust index to compensate for shift
                    } else {
                        buf[len] = '\0';
                        // Strip trailing newline or carriage return
                        buf[strcspn(buf, "\r\n")] = '\0';
                        handle_client_message(fds[i].fd, buf);
                    }
                }
            }
        }
    }

    close(listener);
    return 0;
}