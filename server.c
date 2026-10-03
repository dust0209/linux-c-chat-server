#include <stdio.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <sys/epoll.h>
#include <unistd.h>

int main(void){
    
    int server_fd;
    struct sockaddr_in server_addr;

    server_fd = socket(AF_INET, SOCK_STREAM, 0);

    if (server_fd == -1) {
        perror("socket error");
        return 1;
    }

    int opt = 1;

    if (setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) == -1) {
        perror("setsockopt error");
        return 1;
    }

    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(8080);
    server_addr.sin_addr.s_addr = htonl(INADDR_ANY);

    if (bind(server_fd, (struct sockaddr *)&server_addr, sizeof(server_addr)) == -1) {
        perror("bind error");
        return 1;
    }

    if (listen(server_fd, 5) == -1) {
        perror("listen error");
        return 1;
    }

    int epoll_fd;
    struct epoll_event event;

    epoll_fd = epoll_create1(0);

    if (epoll_fd == -1) {
        perror("epoll_create1 error");
        return 1;
    }

    event.events = EPOLLIN;
    event.data.fd = server_fd;

    if (epoll_ctl(epoll_fd, EPOLL_CTL_ADD, server_fd, &event) == -1) {
        perror("epoll_ctl error");
        return 1;
    }

    struct epoll_event events[10];
    int event_count;

    while (1) {
    
        event_count = epoll_wait(epoll_fd, events, 10, -1);

        if (event_count == -1) {
            perror("epoll_wait error");
            return 1;
        }

        for (int i = 0; i < event_count; i++) {

            if (events[i].data.fd == server_fd) {
            
                int client_fd = accept(server_fd, NULL, NULL);

                if (client_fd == -1) {
                    perror("accept error");
                    return 1;
                }

                printf("Client connected! client_fd = %d\n", client_fd);

                event.events = EPOLLIN;
                event.data.fd = client_fd;

                if (epoll_ctl(epoll_fd, EPOLL_CTL_ADD, client_fd, &event) == -1) {
                    perror("epoll_ctl client error");
                    return 1;
                }
            }
        
            else {

                int current_fd = events[i].data.fd;

                char buffer[1024];
                ssize_t bytes_received;

                bytes_received = recv(current_fd, buffer, sizeof(buffer) - 1, 0); 
            
                if (bytes_received > 0) {
                    buffer[bytes_received] = '\0';
                    printf("Received %zd bytes\n", bytes_received);
                    printf("Message: %s", buffer);
                }
                else if (bytes_received == 0) {
                    printf("Client disconnected\n");
                
                    epoll_ctl(epoll_fd, EPOLL_CTL_DEL, current_fd, NULL);
                    close(current_fd);
                }
                else {
                    perror("recv error");
                    return 1;
                }
            }
        }
    }

    return 0;

}
