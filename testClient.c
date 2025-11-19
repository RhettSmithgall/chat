
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

int listen_port, talk_port;

void *listen_thread(void *arg) {
    int sock = socket(AF_INET, SOCK_STREAM, 0);
    struct sockaddr_in self, other;
    socklen_t len = sizeof(other);

    memset(&self, 0, sizeof(self));
    self.sin_family = AF_INET;
    self.sin_addr.s_addr = htonl(INADDR_ANY);
    self.sin_port = htons(listen_port);

    bind(sock, (struct sockaddr *)&self, sizeof(self));
    listen(sock, 1);

    int conn = accept(sock, (struct sockaddr *)&other, &len);
    printf("Listen connected.\n");

    char buf[256];
    while (1) {
        memset(buf, 0, sizeof(buf));
        int n = read(conn, buf, sizeof(buf));
        if (n <= 0) break;
        printf("[Partner] %s\n", buf);
    }

    close(conn);
    close(sock);
    return NULL;
}

void *talk_thread(void *arg) {
    int sock = socket(AF_INET, SOCK_STREAM, 0);
    struct sockaddr_in other;

    memset(&other, 0, sizeof(other));
    other.sin_family = AF_INET;
    other.sin_addr.s_addr = inet_addr("127.0.0.1"); // LAN or remote IP
    other.sin_port = htons(talk_port);

    // retry until other side is ready
    while (connect(sock, (struct sockaddr *)&other, sizeof(other)) < 0) {
        usleep(200000);
    }

    printf("Talk connected.\n");

    char buf[256];
    while (1) {
        fgets(buf, sizeof(buf), stdin);
        write(sock, buf, strlen(buf));
    }

    close(sock);
    return NULL;
}

int main(int argc, char *argv[]) {
    int sock = socket(AF_INET, SOCK_STREAM, 0);
    struct sockaddr_in serv;

    memset(&serv, 0, sizeof(serv));
    serv.sin_family = AF_INET;
    serv.sin_addr.s_addr = inet_addr("127.0.0.1");
    serv.sin_port = htons(SERVER_PORT);

    connect(sock, (struct sockaddr *)&serv, sizeof(serv));
    printf("Connected to server. Waiting for ports...\n");

    int ports[2];
    read(sock, ports, sizeof(ports));

    listen_port = ports[0];
    talk_port = ports[1];

    printf("Got ports: listen=%d talk=%d\n", listen_port, talk_port);

    pthread_t lt, tt;
    pthread_create(&lt, NULL, listen_thread, NULL);
    usleep(200000); 
    pthread_create(&tt, NULL, talk_thread, NULL);

    pthread_join(lt, NULL);
    pthread_join(tt, NULL);

    return 0;
}
