#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <poll.h>

#define KNRM  "\x27[0m"
#define KRED  "\x27[31m"
#define KGRN  "\x27[32m"
#define KYEL  "\x27[33m"
#define KBLU  "\x27[34m"
#define KMAG  "\x27[35m"
#define KCYN  "\x27[36m"
#define KWHT  "\x27[37m"

#define SERVER_HOSTNAME "localhost" //zos.ospreys.biz
#define SERVER_PORT "1112" //50074

//a struct to hold users
struct user{
    char username[24]; //their username
    int fd; //the server-given fd
    int partner; //the fd of the partner
    int status; //-1 offline, 0 online, 1 active, 2 pending, 3 private
    struct user* next;
};

struct message{
    char chat[1024];
    struct message* next;
};

//GLOBAL VARIABLES
struct user* userlist = NULL;
struct message* chatlog = NULL;
int myFd; //the fd the server has assigned you

struct user* createUser(char* username, int fd, int partner, int status) { 
    struct user* newNode = (struct user*)malloc(sizeof(struct user)); 

    strcpy(newNode->username, username);
    newNode->fd = fd;
    newNode->partner = partner;
    newNode->status = status;
    newNode->next = NULL;
    return newNode;
}

/*
*insertUser(username,fd,partner,status)
*/
struct user* insertUser(struct user** head, char* username, int fd, int partner, int status) { //*head is a pointer to the pointer of the head
    struct user* newSymbol = createUser(username, fd, partner, status);
    if (*head == NULL) {
        *head = newSymbol;
        return newSymbol;
    }
    struct user* temp = *head;
    while (temp->next != NULL) {
        temp = temp->next;
    }
    temp->next = newSymbol;
    return newSymbol;
}

void print_userlist(){
    struct user* temp = userlist;
    printf("active users\n");
    printf("-----------------------------\n");
    while(temp != NULL) {
        if(temp->status == 1){
            if(temp->fd == myFd){
                printf("%s%s%s\n",KYEL,temp->username,KNRM);
            }
            else{
                printf("%s%s%s\n",KBLU,temp->username,KNRM);
            }
        }
        temp = temp->next;
    }
    temp = userlist;
    printf("private chat\n");
    printf("-----------------------------\n");
    while(temp != NULL) {
        if(temp->status == 3){
            if(temp->fd == myFd){
                printf("%s%s%s\n",KYEL,temp->username,KNRM);
            }
            else{
                printf("%s%s%s\n",KGRN,temp->username,KNRM);
            }
        }
        temp = temp->next;
    }
    temp = userlist;
    printf("online\n");
    printf("-----------------------------\n");
    while(temp != NULL) {
        if(temp->status == 0){
            if(temp->fd == myFd){
                printf("%s%s%s\n",KYEL,temp->username,KNRM);
            }
            else{
                printf("%s%s%s\n",KCYN,temp->username,KNRM);
            }
        }
        temp = temp->next;
    }
}

int updateUser(struct user* head,int fd,const char* username,int partner,int status){
    struct user* curr = head;

    while (curr != NULL) {
        if (curr->fd == fd) {

            // Update all fields
            strncpy(curr->username, username, sizeof(curr->username) - 1);
            curr->username[sizeof(curr->username) - 1] = '\0';

            curr->partner = partner;
            curr->status  = status;

            return 1;  // success
        }
        curr = curr->next;
    }

    return 0; // not found
}


void removeUser(struct user* head, int fd) { //*head is a pointer to the pointer of the head
    struct user* temp;
    struct user* last;

    temp = head;
    if(temp == NULL){
        return;
    }

    while(temp->fd != fd){
        if(temp->next == NULL){
            return;
        }
        last = temp;
        temp = temp->next;
    }

    last->next = temp->next;
    free(temp);

    print_userlist();
}

// get sockaddr, IPv4 or IPv6:
void *get_in_addr(struct sockaddr *sa)
{
    if (sa->sa_family == AF_INET) {
        return &(((struct sockaddr_in*)sa)->sin_addr);
    }

    return &(((struct sockaddr_in6*)sa)->sin6_addr);
}

//connects to server then returns the server fd
int connect_to_server(const char *host, const char *port)
{
    struct addrinfo hints, *servinfo, *p;
    int rv;
    int sockfd = -1;
    char ipstr[INET6_ADDRSTRLEN];

    memset(&hints, 0, sizeof hints);
    hints.ai_family = AF_UNSPEC;      // IPv4 or IPv6
    hints.ai_socktype = SOCK_STREAM;  // TCP

    if ((rv = getaddrinfo(host, port, &hints, &servinfo)) != 0) {
        fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(rv));
        return -1;
    }

    // loop through all results and connect to the first we can
    for (p = servinfo; p != NULL; p = p->ai_next) {

        // Make a socket
        sockfd = socket(p->ai_family, p->ai_socktype, p->ai_protocol);
        if (sockfd == -1) {
            perror("socket");
            continue;
        }

        // Print what we're trying to connect to
        inet_ntop(p->ai_family,
                  get_in_addr((struct sockaddr *)p->ai_addr),
                  ipstr, sizeof ipstr);
        printf("client: attempting connection to %s...\n", ipstr);

        // Try to connect
        if (connect(sockfd, p->ai_addr, p->ai_addrlen) == -1) {
            perror("connect");
            close(sockfd);
            sockfd = -1;
            continue;
        }

        // Success!
        break;
    }

    freeaddrinfo(servinfo);

    if (sockfd == -1) {
        fprintf(stderr, "client: failed to connect to server\n");
    }

    return sockfd;
}

void parse_user(const char buffer[32]) {
    char username[12];
    int fd, partner, status;
    int matched = sscanf(buffer, "%11[^,],%d,%d,%d", 
                         username, &fd, &partner, &status);
    if (matched == 4) {
        insertUser(&userlist, username, fd, partner, status);
    }
}

void change_user(char buffer[32]){

    int i = 0;
    while(buffer[i] != '}'){
        i++;
    }
    i++;
    buffer[i] = '\0';

    char username[12];
    int fd, partner, status;
    int matched = sscanf(buffer, "{%11[^,],%d,%d,%d}", 
                         username, &fd, &partner, &status);
    if (matched == 4) {
        if(status < 0){
            removeUser(userlist,fd);
        }
        else{
            updateUser(userlist,fd,username,partner,status);
        }
    }
}

void get_userlist(int server_fd){
    int flag = 1;
    while(flag == 1){
        char buf[32];
        char c;

        recv(server_fd, &c, 1, 0);
        while(c == '{'){
            int i = 0;
            recv(server_fd, &c, 1, 0);
            while(c != '}'){
                buf[i] = c;
                i++;
                recv(server_fd, &c, 1, 0);
            }
            if(strcmp(buf,"END") == 0){
                flag = 0;
                break;
            }
            else{
                parse_user(buf);
                memset(buf,0,strlen(buf));
                recv(server_fd, &c, 1, 0);
            }
        }
    }

    //we are the last user
    struct user* temp = userlist;
    while(temp->next != NULL) {
        temp = temp->next;
    }
    myFd = temp->fd;
    printf("my fd:%d\n",myFd);
}

int main(int argc,char* argv[]){

    //join server
    int server_fd = connect_to_server(SERVER_HOSTNAME,SERVER_PORT);
    if(server_fd < 0){
        return -1;
    }
    printf("connected!\n");

    char username[24];
    strcpy(username,getlogin());
    printf("username: %s\n",username);
    send(server_fd, username, sizeof(username), 0);

    get_userlist(server_fd);

    print_userlist();

    printf("ready to chat!\n");

    struct pollfd fds[2] = {{0,POLLIN,0},{server_fd,POLLIN,0}};

    for (;;) {
        char buffer[256] = { 0 };

        poll(fds, 2, 50000);

        if (fds[0].revents & POLLIN) {
            read(0, buffer, 255);
            send(server_fd, buffer, 255, 0);
        } else if (fds[1].revents & POLLIN) {
            int numbytes = recv(server_fd, buffer, 255, 0);
            if(numbytes < 0){
                return 0;
            }
            buffer[numbytes] = '\0';

            if(buffer[0] == '{'){
                //change in user
                change_user(buffer);
                char *end = strchr(buffer, '}');
                if (end) {
                    end++; // skip '}'
                    memmove(buffer, end, strlen(end) + 1);
                }
            }

            printf("%s\n", buffer);
        }
    }



    //we now have enough information to generate an ncurses window

    //server sends new user
    //update userlist
    //server sends user leaves
    //update userlist

    //server sends chat request
    //prompt user to accept/decline
    //send accept/decline to server

    //user sends message
    //send to server

    //server broadcasts message
    //display message

    //send chat request
    //server responds with pending
    //server responds with accept
    //change view to 1 on 1, all messages you recieve are now from partner
    //send command to leave 1 on 1, or partner leaves you
    //change view back to main chat

    //leave server
    return 0;
    
}
