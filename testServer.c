
#ifndef _XOPEN_SOURCE
#define _XOPEN_SOURCE 600 // Or a higher value like 700 
#endif

#include <arpa/inet.h>
#include <stdio.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>
#include <stdlib.h>
#include <pthread.h>
#include <time.h>

#define SERVER_PORT 5000

int client_fds[2];
pthread_mutex_t lock = PTHREAD_MUTEX_INITIALIZER;
int count = 0;

int get_free_port() {
    int sock = socket(AF_INET, SOCK_STREAM, 0);
    struct sockaddr_in addr;
    socklen_t len = sizeof(addr);

    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_ANY);
    addr.sin_port = 0;  // Let OS pick a port

    bind(sock, (struct sockaddr *)&addr, sizeof(addr));
    getsockname(sock, (struct sockaddr *)&addr, &len);

    int port = ntohs(addr.sin_port);
    close(sock);
    return port;
}

void *client_thread(void *arg) {
    int fd = *(int*)arg;
    free(arg);

    pthread_mutex_lock(&lock);
    client_fds[count++] = fd;

    if (count == 2) {
        int port_listen = get_free_port();
        int port_talk   = get_free_port();

        
        int ports[2];

        ports[0] = port_listen;
        ports[1] = port_talk;
        write(client_fds[0], ports, sizeof(ports));

        ports[0] = port_talk;
        ports[1] = port_listen;
        write(client_fds[1], ports, sizeof(ports));
        
        // Reset and let next 2 players match
        count = 0;
    }

    pthread_mutex_unlock(&lock);

    return NULL;
}

int main() {
    int server_fd, *newfd;
    struct sockaddr_in serv, cli;
    pthread_t t;

    server_fd = socket(AF_INET, SOCK_STREAM, 0);

    memset(&serv, 0, sizeof(serv));
    serv.sin_family = AF_INET;
    serv.sin_addr.s_addr = htonl(INADDR_ANY);
    serv.sin_port = htons(SERVER_PORT);

    bind(server_fd, (struct sockaddr *)&serv, sizeof(serv));
    listen(server_fd, 10);

    printf("Matchmaker server listening on %d...\n", SERVER_PORT);

    while (1) {
        socklen_t len = sizeof(cli);
        newfd = malloc(sizeof(int));
        *newfd = accept(server_fd, (struct sockaddr *)&cli, &len);
        pthread_create(&t, NULL, client_thread, newfd);
        pthread_detach(t);
    }

    return 0;
}
