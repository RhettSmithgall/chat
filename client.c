#include <winsock.h>
#include <ws2tcpip.h>

//#include <arpa/inet.h>
#include <stdio.h>
#include <string.h>
//#include <sys/socket.h>
//#include <unistd.h>
#include <stdlib.h>
//#include <pthread.h>
#include <time.h>

#define SERVER_HOSTNAME "zos.ospreys.biz"
#define SERVER_PORT 50074

//this struct holds all the arguments required for sendRecieve
typedef struct{
    int port;
    char addr[30];
}netStruct;

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


    
}





