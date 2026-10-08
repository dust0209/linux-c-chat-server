#include <stdio.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <string.h>
#include <poll.h>

int main(void) 
{
    int client_fd;
    struct sockaddr_in server_addr;
    char message[1024];
    size_t total_sent = 0;
    size_t message_length;
    char recv_buffer[1024];
    struct pollfd fds[2];

    client_fd = socket(AF_INET, SOCK_STREAM, 0);

    if (client_fd == -1) {
        perror("socket error");
        return 1;
    }

    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(8080);
    if (inet_pton(AF_INET, "127.0.0.1", &server_addr.sin_addr) != 1) {
        fprintf(stderr, "Invalid server IP address\n");
        close(client_fd);
        return 1;
    }

    if (connect(
        client_fd,
        (struct sockaddr *)&server_addr,
        sizeof(server_addr)
    ) == -1) {
        perror("connect error");
        close(client_fd);
        return 1;   
    }

    printf("Connected to server\n");
    
    fds[0].fd = STDIN_FILENO;
    fds[0].events = POLLIN;

    fds[1].fd = client_fd;
    fds[1].events = POLLIN;

    

    while (1) {

        int ready = poll(fds, 2, -1);

        if (ready == -1) {
            perror("Poll error");
            close(client_fd);
            return 1;
        }

        if (fds[0].revents & POLLIN) {
            if (fgets(message, sizeof(message), stdin) == NULL) {
                printf("Input ended\n");
                break;
            }
            message_length = strlen(message);
            total_sent = 0;

            while (total_sent < message_length) {
                ssize_t sent = send(
                    client_fd, 
                    message + total_sent, 
                    message_length - total_sent, 
                    0);

                if (sent <= 0) {
                    perror("send error");
                    close(client_fd);
                    return 1;
                }

                total_sent += sent;
            }
            
            printf("Sent %zu bytes\n", total_sent);
        }

        if (fds[1].revents & POLLIN) {
       
            ssize_t received = recv(
                client_fd, 
                recv_buffer, 
                sizeof(recv_buffer) - 1, 
                0
            );

            if (received > 0) {
                printf("Received %zd bytes\n", received);
                fwrite(recv_buffer, 1, received, stdout);
            } else if (received == 0) {
                printf("Server disconnected\n");
                break;
            } else {
                perror("recv error");
            }
        }
    }

    close(client_fd);
    return 0;
}