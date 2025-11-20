#include <winsock.h>
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include <sys/types.h>
#include <signal.h>
#include <ws2tcpip.h>

#define SERVER_HOSTNAME "zos.ospreys.biz"
#define SERVER_PORT 50074
#define BACKLOG 20

int main( int argc, char *argv[]){

    struct sockaddr_in server_addr;

    struct sockaddr_in their_addr;
    socklen_t addr_size;

    //create socket
    int server_fd;
    if((server_fd = socket(PF_INET,SOCK_STREAM, 0)) < 0){
        perror("Error creating socket");
        exit(1);
    }

    //populate socket with addresses
    server_addr.sin_family = AF_INET;                         // host byte order
    server_addr.sin_port = htons(SERVER_PORT);                // short, network byte order
    server_addr.sin_addr.s_addr = inet_addr(SERVER_HOSTNAME); // ip address
    memset(&(server_addr.sin_zero), '\0', 8);                 // zero the rest of the struct

    //bind the socket to the struct of values
    if (bind(server_fd,(struct sockaddr *)&server_addr,sizeof(struct sockaddr)) < 0){
        perror("Bind Error");
        exit(1);
    }

    int yes = 1;
    setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &yes,sizeof(int));

    //wait and listen for a connection
    if (listen(server_fd, BACKLOG) < 0){
        perror("Listen Error");
        exit(1);
    }

    int new_fd;
    addr_size = sizeof(their_addr);

    while(1){
        if(new_fd = accept(server_fd, (struct sockaddr *)&their_addr,&addr_size) < 0){
            perror("Accept Error");
            continue;
        };

        
    }
    
    


}