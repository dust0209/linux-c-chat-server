#include <stdio.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <netinet/in.h>

int main(void){
    
    int server_fd;
    struct sockaddr_in server_addr;

    server_fd = socket(AF_INET, SOCK_STREAM, 0);

    if (server_fd == -1){
        perror("socket error");
        return 1;
    }

    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(8080);
    server_addr.sin_addr.s_addr = htonl(INADDR_ANY);

    if (bind(server_fd, (struct sockaddr *)&server_addr, sizeof(server_addr)) == -1){
        perror("bind error");
        return 1;
    }

    if (listen(server_fd, 5) == -1){
        perror("listen error");
        return 1;
    }

    int client_fd;

    client_fd = accept(server_fd, NULL, NULL);

    if (client_fd == -1){
        perror("accept error");
        return 1;
    }

    printf("Client connected! client_fd = %d\n", client_fd);

    return 0;

}
