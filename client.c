#include <arpa/inet.h>
#include <stdio.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>
#include <stdlib.h>
#include <pthread.h>
#include <time.h>

//BASIC STRUCTURE OF CLIENT
//1. connect to server through a set, hardcoded port and host
//2. recieve a new host and port from server
//3. connect through that to another client
//4. chat with client

#define hostname "zos.ospreys.biz"
#define port 50074

//this struct holds all the arguments required for sendRecieve
typedef struct{
    int input;
    int port;
    char addr[30];
}netStruct;

//function declaration
void ThreadManager(int,struct myStructure);  //creates threads that run sendReceive
int mainMenu();                              //returns the user input and displays mainmenu
int howManyThreads();                        //manages user input and returns how many threads are needed
void* sendReceive(void*);                    //sends input to the server then prints the output from the server

int main( int argc, char *argv[]){


    netStruct server;
    strcpy(server.Addr,hostname);   //hostname
    server.port = port;             //port number

    int sockfd;

    //create socket
    if((sockfd = socket(PF_INET,SOCK_STREAM, 0)) < 0){
        perror("Error creating socket");
        exit(1);
    }

    //populate socket with addresses
    dest_addr.sin_family = AF_INET;                     // host byte order
    dest_addr.sin_port = htons(server.port);        // short, network byte order
    dest_addr.sin_addr.s_addr = inet_addr(server.addr); //ip address
    memset(&(dest_addr.sin_zero), '\0', 8);             // zero the rest of the struct

    //connect and wait for accept() on the server side
    if(connect(sockfd, (struct sockaddr *)&dest_addr, sizeof(struct sockaddr)) < 0){
        perror("Connection error");
    }
    printf("Connected to server");

    int numbytes = 0;
    char buffer[128];
    int userInput[10];

    while(numbytes = recv(sockfd,buffer,sizeof(buffer),0) > 0){ //recieve and print output from server
        printf("recieving: %s",buffer); 

        //the server will send 2 ports, a talking port first, and a listening port 2nd
        int talkPort = atoi(strtok(buffer," "));
        int listenPort = atoi(strtok(NULL," "));

        struct mystructure talkAddr;

        talkAddr.port = talkPort;
        pthread_create(&thread_id[1],NULL,sendReceive,(void *)&talkAddr);
        
    }
    if (numbytes == -1) {
        perror("recv"); 
    }
    else if (numbytes == 0) {//close socket when connection has been terminated
        close(sockfd); 
    }
}

//a thread that opens a socket to listen
void* listenThread(void *input){
    int sockfd;

    //derefrencing our void pointer structure with all the port and ip address information
    struct myStructure *theirAddr = (struct myStructure*)input;
    struct sockaddr_in dest_addr;

    //create socket
    if((sockfd = socket(PF_INET,SOCK_STREAM, 0)) < 0){
        perror("Error creating socket");
        exit(1);
    }

    //populate socket with addresses
    dest_addr.sin_family = AF_INET;                     // host byte order
    dest_addr.sin_port = htons(theirAddr->port);        // short, network byte order
    dest_addr.sin_addr.s_addr = inet_addr(theirAddr->theirAddr); //ip address
    memset(&(dest_addr.sin_zero), '\0', 8);             // zero the rest of the struct

    //connect and wait for accept() on the server side
    if(connect(sockfd, (struct sockaddr *)&dest_addr, sizeof(struct sockaddr)) < 0){
        perror("Connection error");
    }

    int numbytes = 0;
    char buffer[128];
    int userInput[10];

    //send input to server
    if (send(sockfd,1,sizeof(userInput), 0) == -1){
        perror("send");
        exit(1);
    }

    while(numbytes = recv(sockfd,buffer,sizeof(buffer),0) > 0){ //recieve and print output from server
        printf("recieving: %s",buffer); 
    }
    if (numbytes == -1) {
        perror("recv"); 
    }
    else if (numbytes == 0) {//close socket when connection has been terminated
        close(sockfd); 
    }

    pthread_cancel(pthread_self());
}

//a thread that opens a socket to talk
void *talkThread(void *input){
    int sockfd;

    //derefrencing our void pointer structure with all the port and ip address information
    struct myStructure *theirAddr = (struct myStructure*)input;
    struct sockaddr_in dest_addr;

    //create socket
    if((sockfd = socket(PF_INET,SOCK_STREAM, 0)) < 0){
        perror("Error creating socket");
        exit(1);
    }

    //populate socket with addresses
    dest_addr.sin_family = AF_INET;                     // host byte order
    dest_addr.sin_port = htons(theirAddr->port);        // short, network byte order
    dest_addr.sin_addr.s_addr = inet_addr(theirAddr->theirAddr); //ip address
    memset(&(dest_addr.sin_zero), '\0', 8);             // zero the rest of the struct

    //connect and wait for accept() on the server side
    if(connect(sockfd, (struct sockaddr *)&dest_addr, sizeof(struct sockaddr)) < 0){
        perror("Connection error");
    }

    int numbytes = 0;
    char buffer[128];
    int userInput[10];

    //send input to server
    if (send(sockfd,1,sizeof(userInput), 0) == -1){
        perror("send");
        exit(1);
    }

    while(numbytes = recv(sockfd,buffer,sizeof(buffer),0) > 0){ //recieve and print output from server
        printf("recieving: %s",buffer); 
    }
    if (numbytes == -1) {
        perror("recv"); 
    }
    else if (numbytes == 0) {//close socket when connection has been terminated
        close(sockfd); 
    }

    pthread_cancel(pthread_self());
}



//sendreceive is the thread function the threadmanager runs
void *sendReceive(void *input){
    int sockfd;

    //derefrencing our void pointer structure with all the port and ip address information
    struct myStructure *theirAddr = (struct myStructure*)input;
    struct sockaddr_in dest_addr;

    //create socket
    if((sockfd = socket(PF_INET,SOCK_STREAM, 0)) < 0){
        perror("Error creating socket");
        exit(1);
    }

    //populate socket with addresses
    dest_addr.sin_family = AF_INET;                     // host byte order
    dest_addr.sin_port = htons(theirAddr->port);        // short, network byte order
    dest_addr.sin_addr.s_addr = inet_addr(theirAddr->theirAddr); //ip address
    memset(&(dest_addr.sin_zero), '\0', 8);             // zero the rest of the struct

    //connect and wait for accept() on the server side
    if(connect(sockfd, (struct sockaddr *)&dest_addr, sizeof(struct sockaddr)) < 0){
        perror("Connection error");
    }

    int numbytes = 0;
    char buffer[128];
    int userInput[10];

    //send input to server
    if (send(sockfd,1,sizeof(userInput), 0) == -1){
        perror("send");
        exit(1);
    }

    while(numbytes = recv(sockfd,buffer,sizeof(buffer),0) > 0){ //recieve and print output from server
        printf("recieving: %s",buffer); 
    }
    if (numbytes == -1) {
        perror("recv"); 
    }
    else if (numbytes == 0) {//close socket when connection has been terminated
        close(sockfd); 
    }

    pthread_cancel(pthread_self());
}

void ThreadManager(struct myStructure inputAddr)
{
    pthread_t thread_id[1];

    //each thread needs to be able to record it's own individual time to complete so
    //an array of structs are created to be arguments for our threaded function
    struct myStructure theirAddr[1];
    int numberOfThreads = 0;

    pthread_create(&thread_id[1],NULL,sendReceive,(void *)&theirAddr[i]);
           
    pthread_join(thread_id[1],NULL);
}
