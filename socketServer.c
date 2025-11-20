#include <arpa/inet.h>
#include <stdio.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>
#include <stdlib.h>
#include <pthread.h>
#define BACKLOG 20

//this struct holds all the arguments required for sendRecieve
struct myStructure {
    int* numberofThreads;
    int newfd;
};

//function declaration
void commandLine(int,int); //collects server command line input from commands such as ls, date and more
int createSocket(); //creates a socket
int listenConnect(int); //listens for connection and returns the new socket fd when a connection has been made
void* threadedFunction(void*);
void helloUser();
void goodbyeUser();

//main
int main( int argc, char *argv[]){
    int buffer[64];
    int numbytes,exitFlag,port;
    int sockfd, sin_size;
    int numberofThreads = 0;

    char theirAddr[INET_ADDRSTRLEN];
    int theirPort;

    struct sockaddr_in my_addr;
    struct sockaddr_in their_addr;
    sin_size = sizeof(struct sockaddr_in);

    //the command line arguments come in as char arrays so atoi will parse that into an int for port
    port = atoi(argv[1]);

    //create a socket
    if((sockfd = socket(PF_INET,SOCK_STREAM, 0)) < 0){
        perror("Error creating socket");
        exit(1);
    }

    //assign it values
    my_addr.sin_family = AF_INET;       // host byte order
    my_addr.sin_port = htons(port);     // short, network byte order
    my_addr.sin_addr.s_addr = INADDR_ANY; //this machines ip address
    memset(&(my_addr.sin_zero), '\0', 8); // zero the rest of the struct

    //bind the socket to the struct of values
    if (bind(sockfd,(struct sockaddr *)&my_addr,sizeof(struct sockaddr)) < 0){
        perror("Bind Error");
        exit(1);
    }


    //wait and listen for a connection
    if (listen(sockfd, BACKLOG) < 0){
        perror("Listen Error");
        exit(1);
    }

    helloUser();

    int i = 0;
    pthread_t thread_id[100];
    struct myStructure myFd[100];

    do{
        //to ensure there are only 12 threads at any given time
        if(numberofThreads < 13){
            printf("| Listening on port: %4i                                 |\n",port);

            //accept a connection and create a new socket to collect the info from
            if ((myFd[i].newfd = accept(sockfd, (struct sockaddr *)&their_addr,&sin_size)) < 0){
                perror("Accept Error");
                exit(1);
            }

            getpeername(sockfd, (struct sockaddr *)&their_addr,&sin_size);
            inet_ntop(AF_INET, &(their_addr.sin_addr), theirAddr, INET_ADDRSTRLEN);
            theirPort = ntohs(their_addr.sin_port);
            printf("| Processing request from: %s:%i       |\n",theirAddr,theirPort);

            //this stores the address of numberofThreads in the struct we send as an argument to our threaded function
            //so it can call back and update the number of running threads when its done
            myFd[i].numberofThreads = &numberofThreads;
            pthread_create(&thread_id[i],NULL,threadedFunction,(void*)&myFd[i]);
            i++;
        }
        else
        {
            i--;
        }

        if(i>99){
            i=0;
        }

    }while(buffer[0] != 7);

    goodbyeUser();
}

void* threadedFunction(void* input){
    struct myStructure *theirAddr = (struct myStructure*)input;

    int numbytes;
    int buffer[64];
    int newFD = theirAddr->newfd;

    //recieve from the client and pass the input to commandline()
    if((numbytes=recv(newFD, buffer, 99, 0)) != 0){
        buffer[numbytes] = '\0';
        commandLine(buffer[0],newFD);
    }

     *(theirAddr->numberofThreads)--;

     pthread_cancel(pthread_self());
}

//popen is a function that opens a "pipe" to the command line that can be written and read from
//the following function (with some modification) is taken from:
//https://stackoverflow.com/questions/4757512/execute-a-linux-command-in-the-c-program
void commandLine(int input,int newfd){
    char command[20];

    switch(input) {
    case 1:
        strcpy(command,"date");
        break;
    case 2:
        strcpy(command,"uptime");
        break;
    case 3:
        strcpy(command,"free");
        break;
    case 4:
        strcpy(command,"netstat");
        break;
    case 5:
        strcpy(command,"who");
        break;
    case 6:
        strcpy(command,"ps");
        break;
    default:
        return;
        break; }

    FILE *pf;
    char data[128];

    // Setup our pipe for reading and execute our command.
    pf = popen(command,"r");

    // collect lines from the command line and send them to the client until nothing is left in the buffer
    while(fgets(data, 128 , pf)){
        if (send(newfd,data,sizeof(data),0) < 0){
            perror("send");
            exit(1);
        }
    }

    if (pclose(pf) != 0){
        fprintf(stderr," Error: Failed to close command stream \n");
    }

    close(newfd); //close connection with client
}

void helloUser(){
    printf("|---------------------------------------------------------|\n");
    printf("|        Rhett Smithgall and George Shannon               |\n");
    printf("|                Multithreaded Server                     |\n");
    printf("|---------------------------------------------------------|\n");
}

void goodbyeUser(){
    printf("\n|---------------------------------------------------------|\n");
    //Autograph
    printf("Programmer:\n");
    printf(""
            "    ____  __         __  __     _____           _ __  __                ____    \n"
            "   / __ \\/ /_  ___  / /_/ /_   / ___/____ ___  (_) /_/ /_  ____ _____ _/ / /    \n"
            "  / /_/ / __ \\/ _ \\/ __/ __/   \\__ \\/ __ `__ \\/ / __/ __ \\/ __ `/ __ `/ / /     \n"
            " / _, _/ / / /  __/ /_/ /_    ___/ / / / / / / / /_/ / / / /_/ / /_/ / / /      \n"
            "/_/ |_/_/ /_/\\___/\\__/\\__/   /____/_/ /_/ /_/_/\\__/_/ /_/\\__, /\\__,_/_/_/       \n"
            "                                                        /____/                  \n");
}