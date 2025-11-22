#include "headers.h"

//global struct to hold active users


int main( int argc, char *argv[]){

    struct sockaddr_in server_addr;

    //create socket
    int server_fd;
    if((server_fd = socket(PF_INET,SOCK_STREAM, 0)) < 0){
        perror("socket() error");
        exit(1);
    }

    //populate socket with addresses
    server_addr.sin_family = AF_INET;                         // host byte order
    server_addr.sin_port = htons(SERVER_PORT);                // short, network byte order
    server_addr.sin_addr.s_addr = inet_addr(SERVER_HOSTNAME); // ip address
    memset(&(server_addr.sin_zero), '\0', 8);                 // zero the rest of the struct

    if(connect(server_fd, (struct sockaddr *)&server_addr, sizeof(struct sockaddr)) < 0){
        perror("Connect() error");
    }

    User* activeUser = NULL;

    pthread_t tid;
    pthread_create(&tid, NULL, recv_thread, &server_fd);

    char msg[1024];
    while (fgets(msg, sizeof(msg), stdin)) {

        send(server_fd, msg, strlen(msg), 0);
    }

}

void *recv_thread(void *arg) {
    int sock = *(int*)arg;
    char buffer[1024];

    while (1) {
        int n = recv(sock, buffer, sizeof(buffer)-1, 0);
        if (n <= 0){break;}
        buffer[n] = '\0';

        msgHandler(buffer);
    }
    return NULL;
}

void msgHandler(const char *msg) {
    char *token = strtok(msg, " ");
    if(strcmp(token, "chat") == 0){ //if the packet is a chat message
        printf("%s", token);

    }
    else if(strcmp(token, "user") == 0){ //if the packet is the list of users
        printf("Online Users:\n");
        token = strtok(NULL, " ");
        while (token != NULL) {
            printf("%s\n", token);
            token = strtok(NULL, " ");
        }
    }
    else if(strcmp(token, "1on1") == 0){ //tells us we are now 1 on 1 with another client
        printf("Online Users:\n");
        token = strtok(NULL, " ");
        while (token != NULL) {
            printf("%s\n", token);
            token = strtok(NULL, " ");
        }
    }
}





