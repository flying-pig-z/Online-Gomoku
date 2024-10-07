#include <arpa/inet.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/select.h>
#define PORT 8848
#define BUFFER_SIZE 1024

// 创建服务器 socket
int create_server_socket() {
    int server_fd;
    struct sockaddr_in address;
    int opt = 1;

    // 创建 socket
    if ((server_fd = socket(AF_INET, SOCK_STREAM, 0)) == 0) {
        perror("Socket creation error");
        exit(EXIT_FAILURE);
    }

    // 绑定 socket
    setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons(PORT);
    if (bind(server_fd, (struct sockaddr*)&address, sizeof(address)) < 0) {
        perror("绑定socket失败");
        exit(EXIT_FAILURE);
    }

    // 监听
    if (listen(server_fd, 2) < 0) {
        perror("监听失败");
        exit(EXIT_FAILURE);
    }

    printf("服务正在监听端口 %d...\n", PORT);
    return server_fd;
}



// 与客户端通信
void communicate_with_clients(int client1_fd, int client2_fd, int server_fd, struct sockaddr_in *address, socklen_t *addrlen) {
    char buffer[BUFFER_SIZE];
    int max_fd = (client1_fd > client2_fd) ? client1_fd : client2_fd;

    while (1) {
        fd_set readfds;
        FD_ZERO(&readfds);
        FD_SET(client1_fd, &readfds);
        FD_SET(client2_fd, &readfds);

        // 等待数据可读
        int activity = select(max_fd + 1, &readfds, NULL, NULL, NULL);
        if (activity < 0) {
            perror("select error");
            break;
        }

        // 从客户端 1 读取数据
        if (FD_ISSET(client1_fd, &readfds)) {
            int n = read(client1_fd, buffer, BUFFER_SIZE);
            if (n < 0) {
                perror("Read from Client 1 failed");
                break;
            } else if (n == 0) {
                printf("Client 1 disconnected\n");
                close(client1_fd);
                client1_fd = accept(server_fd, (struct sockaddr*)address, addrlen);
                printf("New Client 1 connected\n");
                continue;  // 继续下一次循环
            }

            buffer[n] = '\0';
            printf("Received from Client 1: %s\n", buffer);
            send(client2_fd, buffer, n, 0);
        }

        // 从客户端 2 读取数据
        if (FD_ISSET(client2_fd, &readfds)) {
            int n = read(client2_fd, buffer, BUFFER_SIZE);
            if (n < 0) {
                perror("Read from Client 2 failed");
                break;
            } else if (n == 0) {
                printf("Client 2 disconnected\n");
                close(client2_fd);
                client2_fd = accept(server_fd, (struct sockaddr*)address, addrlen);
                printf("New Client 2 connected\n");
                continue;  // 继续下一次循环
            }

            buffer[n] = '\0';
            printf("Received from Client 2: %s\n", buffer);
            send(client1_fd, buffer, n, 0);
        }
    }
}

// 处理客户端连接
void handle_client_connections(int server_fd) {
    int client1_fd, client2_fd;
    struct sockaddr_in address;
    socklen_t addrlen = sizeof(address);

    // 接受客户端
    if ((client1_fd = accept(server_fd, (struct sockaddr*)&address, &addrlen)) < 0) {
        perror("接受客户端1失败");
        exit(EXIT_FAILURE);
    }
    printf("客户端1连接成功\n");

    if ((client2_fd = accept(server_fd, (struct sockaddr*)&address, &addrlen)) < 0) {
        perror("接受客户端2失败");
        exit(EXIT_FAILURE);
    }
    printf("客户端2连接成功\n");

    // 开始与客户端通信
    communicate_with_clients(client1_fd, client2_fd, server_fd, &address, &addrlen);

    // 关闭 sockets
    close(client1_fd);
    close(client2_fd);
}



int main() {
    int server_fd = create_server_socket();
    handle_client_connections(server_fd);
    close(server_fd);
    return 0;
}
