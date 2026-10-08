#include <stdio.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <sys/epoll.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <string.h>

#define MESSAGE_BUFFER_SIZE 4096

struct client {
    int fd;
    char message_buffer[MESSAGE_BUFFER_SIZE];
    size_t message_length;
    struct client *next;
};

void remove_client(struct client **client_list, struct client *current_client) {
    if (*client_list == current_client) {
        *client_list = current_client->next;
        return;
    }

    struct client *prev = *client_list;

    while (prev != NULL && prev->next != current_client) {
        prev = prev->next;
    }

    if (prev != NULL) {
        prev->next = current_client->next;
    }
}

void cleanup_client(
    int epoll_fd,
    struct client **client_list,
    struct client *current_client
) {
    int current_fd = current_client->fd;

    if (epoll_ctl(epoll_fd, EPOLL_CTL_DEL, current_fd, NULL) == -1) {
        perror("epoll_ctl DEL error");
    }

    close(current_fd);
    remove_client(client_list, current_client);
    free(current_client);
}

int main(void){
    
    int server_fd;
    struct sockaddr_in server_addr;
    struct client *client_list = NULL;

    server_fd = socket(AF_INET, SOCK_STREAM, 0);

    if (server_fd == -1) {
        perror("socket error");
        return 1;
    }

    int flags = fcntl(server_fd, F_GETFL, 0);

    if (flags == -1) {
        perror("fcntl F_GETFL error");
        return 1;
    }

    if (fcntl(server_fd, F_SETFL, flags | O_NONBLOCK) == -1) {
        perror("fcntl F_SETFL error");
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
    event.data.ptr = NULL;

    if (epoll_ctl(epoll_fd, EPOLL_CTL_ADD, server_fd, &event) == -1) {
        perror("epoll_ctl error");
        return 1;
    }

    struct epoll_event events[10];
    int event_count;

    while (1) {
    
        event_count = epoll_wait(epoll_fd, events, 10, -1);

        if (event_count == -1) {
            if (errno == EINTR) {
                continue;
            }

            perror("epoll_wait error");
            return 1;
        }

        for (int i = 0; i < event_count; i++) {

            if (events[i].data.ptr == NULL) {
            
                while (1) {
                    
                    int client_fd = accept(server_fd, NULL, NULL);

                    if (client_fd == -1) {
                        if (errno == EAGAIN || errno == EWOULDBLOCK) {
                            break;
                        }
                        else if (errno == EINTR) {
                            continue;
                        }
                        else {
                            perror("accept error");
                            return 1;
                        }
                    }

                    struct client *new_client = malloc(sizeof(struct client));

                    if (new_client == NULL) {
                        perror("malloc error");
                        close(client_fd);
                        return 1;
                    }
                    
                    new_client->fd = client_fd;
                    new_client->message_length = 0;
                    new_client->next = client_list;
                    client_list = new_client;

                    int client_flags = fcntl(client_fd, F_GETFL, 0);

                    if (client_flags == -1) {
                        perror("fcntl client F_GETFL error");

                        close(client_fd);
                        remove_client(&client_list, new_client);
                        free(new_client);

                        continue;
                    }

                    if (fcntl(client_fd, F_SETFL, client_flags | O_NONBLOCK) == -1) {
                        perror("fcntl client F_SETFL error");

                        close(client_fd);
                        remove_client(&client_list, new_client);
                        free(new_client);

                        continue;
                    }

                    printf("Client connected! client_fd = %d\n", client_fd);

                    event.events = EPOLLIN;
                    event.data.ptr = new_client;

                    if (epoll_ctl(epoll_fd, EPOLL_CTL_ADD, client_fd, &event) == -1) {
                        perror("epoll_ctl client error");
                        
                        cleanup_client(epoll_fd, &client_list, new_client);
                        
                        continue;
                    }
                }
            }
        
            else {

                struct client *current_client = events[i].data.ptr;
                int current_fd = current_client->fd;

                if (events[i].events & (EPOLLERR | EPOLLHUP)) {
                    printf("Client connection error or hangup\n");

                    cleanup_client(epoll_fd, &client_list, current_client);

                    continue;
                }

                char buffer[1024];
                ssize_t bytes_received;

                while(1) {
                   
                    bytes_received = recv(current_fd, buffer, sizeof(buffer) - 1, 0); 
            
                    if (bytes_received > 0) {
                        buffer[bytes_received] = '\0';
                        printf("Received %zd bytes\n", bytes_received);

                        for (ssize_t j = 0; j < bytes_received; j++) {
                            
                            if (current_client->message_length >= MESSAGE_BUFFER_SIZE - 1) {
                                printf("Message too long\n");
                                current_client->message_length = 0;
                            }
                            
                            current_client->message_buffer[current_client->message_length] = buffer[j];
                            current_client->message_length++;

                            if (buffer[j] == '\n') {
                                current_client->message_buffer[current_client->message_length] = '\0';

                                printf(
                                    "Message from client_fd=%d: %s",
                                    current_fd,
                                    current_client->message_buffer
                                );
                                
                                struct client *target = client_list;

                                while (target != NULL) {
                                    if (target != current_client) {
                                        ssize_t sent = send(
                                            target->fd,
                                            current_client->message_buffer,
                                            current_client->message_length,
                                            MSG_NOSIGNAL
                                        );
 
                                        if (sent == -1 && errno != EAGAIN && errno != EWOULDBLOCK) {
                                            perror("Broadcast send error");
                                        }
                                    }

                                    target = target->next;
                                }

                            current_client->message_length = 0;
                            }
                        }
                    }
                    
                    else if (bytes_received == 0) {
                        printf("Client disconnected\n");
                        
                        cleanup_client(epoll_fd, &client_list, current_client);

                        break;
                    }
                  
                    else {
                        if (errno == EAGAIN || errno == EWOULDBLOCK) {
                            break;
                        }
                        else if (errno == EINTR) {
                            continue;
                        }
                        else {
                            perror("recv error");
                            
                            cleanup_client(epoll_fd, &client_list, current_client);

                            break;
                        }
                    } 
                }  
            }
        }
    }
    
    return 0;

}
