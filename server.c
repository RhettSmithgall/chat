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

#define SERVER_HOSTNAME "zos.ospreys.biz"
#define SERVER_PORT "50074"
#define BACKLOG 20

//GLOBABL VARIABLES
struct user* userlist = NULL;
struct message* chatlog = NULL;

//a struct to hold users
struct user{
    char username[24]; //their username
    int fd; //the server-given fd
    int partner; //the fd of the partner
    int status; //0 online, 1 active, 2 pending, 3 private
    struct user* next;
};

struct message{
    char chat[1024];
    struct message* next;
};

// Remove an index from the set
void del_from_pfds(struct pollfd pfds[], int i, int *fd_count){
    // Copy the one from the end over this one
    pfds[i] = pfds[*fd_count-1];

    (*fd_count)--;
}

// Return a listening socket
int get_listener_socket(void){
    int listener;     // Listening socket descriptor
    int yes=1;        // For setsockopt() SO_REUSEADDR, below
    int rv;

    struct addrinfo hints, *ai, *p;

    // Get us a socket and bind it
    memset(&hints, 0, sizeof hints);
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_flags = AI_PASSIVE;
    if ((rv = getaddrinfo(NULL, SERVER_PORT, &hints, &ai)) != 0) {
        fprintf(stderr, "selectserver: %s\n", gai_strerror(rv));
        exit(1);
    }
    
    for(p = ai; p != NULL; p = p->ai_next) {
        listener = socket(p->ai_family, p->ai_socktype, p->ai_protocol);
        if (listener < 0) { 
            continue;
        }
        
        // Lose the pesky "address already in use" error message
        setsockopt(listener, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(int));

        if (bind(listener, p->ai_addr, p->ai_addrlen) < 0) {
            close(listener);
            continue;
        }

        break;
    }

    // If we got here, it means we didn't get bound
    if (p == NULL) {
        return -1;
    }

    freeaddrinfo(ai); // All done with this

    // Listen
    if (listen(listener, 10) == -1) {
        return -1;
    }

    return listener;
}

int recv_all(int fd, void *buf, int len) {
    int total = 0;
    int n;
    while (total < len) {
        n = recv(fd, (char*)buf + total, len - total, 0);
        if (n <= 0) {

            return n; 
        }
        total += n;
    }
    return 1;  
}

struct user* createUser(char* username, int fd, int partner, int status) { 
    struct user* newNode = (struct user*)malloc(sizeof(struct user)); 

    strcpy(newNode->username, username);
    newNode->fd = fd;
    newNode->partner = partner;
    newNode->status = status;
    newNode->next = NULL;
    return newNode;
}

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

// Add a new file descriptor to the set
void add_to_pfds(struct pollfd *pfds[], int newfd, int *fd_count, int *fd_size){
    // If we don't have room, add more space in the pfds array
    if (*fd_count == *fd_size) {
        *fd_size *= 2; // Double it

        *pfds = realloc(*pfds, sizeof(**pfds) * (*fd_size));
    }

    (*pfds)[*fd_count].fd = newfd;
    (*pfds)[*fd_count].events = POLLIN; // Check ready-to-read

    (*fd_count)++;
}

void add_new_client(int listener,struct pollfd **pfds,int *fd_count,int *fd_size){
    struct sockaddr_storage remoteaddr;
    socklen_t addrlen = sizeof remoteaddr;

    char remoteIP[INET6_ADDRSTRLEN];

    int newfd = accept(listener, (struct sockaddr *)&remoteaddr, &addrlen);
    if (newfd == -1) {
        perror("accept");
        return;
    }

    char username[24];
    int nbytes = recv(newfd, username, sizeof(username), 0);
    username[nbytes] = '\0';

    printf("%s connected!\n");

    insertUser(&userlist,username,newfd,0,1);

    struct user* temp = userlist;
    while(temp != NULL) {
        printf("USER: %s %d %d %d\n",temp->username,temp->fd,temp->partner,temp->status);
        temp = temp->next;
    }

    temp = userlist;
    while(temp != NULL) {
        char msg[48];
        snprintf(msg,sizeof(msg),"{%s,%d,%d,%d}",temp->username,temp->fd,temp->partner,temp->status);
        send(newfd,msg,strlen(msg),0);
        temp = temp->next;
    }
    printf("sending END\n");
    send(newfd,"{END}",5,0);

    add_to_pfds(pfds, newfd, fd_count, fd_size);
}

int main(int argc, char* argv[]){
    int listener;
    struct sockaddr_in my_addr;

    //create a socket
    listener = get_listener_socket();
    if (listener < 0){
        perror("Socket Error");
        exit(1);
    }

    printf("listening on %s:%s\n",SERVER_HOSTNAME,SERVER_PORT);

    //make a struct to hold all the connections
    int fd_count = 0;
    int fd_size = 5;
    struct pollfd *pfds = malloc(sizeof *pfds * fd_size);

    //make lister connection #1
    pfds[0].fd = listener;
    pfds[0].events = POLLIN; 

    fd_count = 1; 

    //0 online, 1 active, 2 pending, 3 private
    insertUser(&userlist, "s990333", 20, 0 , 0);
    insertUser(&userlist, "s990355", 40, 0 , 0);
    insertUser(&userlist, "s990367", 45, 0 , 0);
    insertUser(&userlist, "s990101", 10, 15, 3);
    insertUser(&userlist, "s990123", 15, 10, 3);
    insertUser(&userlist, "s990444", 25, 0 , 1);
    insertUser(&userlist, "s990555", 30, 0 , 1);
    insertUser(&userlist, "s990666", 35, 0 , 1);

    char buf[255];

    for (;;) {

        int poll_count = poll(pfds, fd_count, -1);
        if (poll_count == -1) {
            perror("poll");
            exit(1);
        }

        for (int i = 0; i < fd_count; i++) {

            // Only act when there is data or a new connection
            if (pfds[i].revents & POLLIN) {

                // New client connecting?
                if (pfds[i].fd == listener) {
                    add_new_client(listener, &pfds, &fd_count, &fd_size);
                    continue;
                }

                // Existing client sending something
                int sender_fd = pfds[i].fd;
                int nbytes = recv(sender_fd, buf, sizeof(buf), 0);

                if (nbytes <= 0) {
                    // -------------------------
                    // HANDLE DISCONNECT
                    // -------------------------
                    printf("socket %d disconnected\n", sender_fd);

                    // Find their user entry
                    struct user *temp = userlist;
                    while (temp && temp->fd != sender_fd) {
                        temp = temp->next;
                    }

                    if (temp) {
                        // Broadcast updated user to everyone
                        char notify[128];
                        //snprintf(notify, sizeof(notify),"{%s %d %d %d}",
                            //temp->username,temp->fd,temp->partner,-1);
                        snprintf(buf, sizeof(buf),"%s disconnected\n", temp->username);

                        for (int j = 0; j < fd_count; j++) {
                            int dest_fd = pfds[j].fd;

                            if (dest_fd != listener && dest_fd != sender_fd) {
                                //send(dest_fd, notify, strlen(notify), 0);
                                send(dest_fd, buf, nbytes, 0);
                            }
                        }
                    }

                    // Remove from poll list
                    close(sender_fd);
                    del_from_pfds(pfds, i, &fd_count);
                    i--;   // IMPORTANT: adjust index because pfds shifted
                }
                else {
                    // -------------------------
                    // NORMAL MESSAGE RECV
                    // -------------------------
                    // Broadcast buf to all others
                    for (int j = 0; j < fd_count; j++) {
                        int dest_fd = pfds[j].fd;

                        if (dest_fd != listener && dest_fd != sender_fd) {
                            if (send(dest_fd, buf, nbytes, 0) == -1) {
                                perror("send");
                            }
                        }
                    }
                }
            }
        }
    }

    //new connection
    //make new fd for new user, accept()
    //get username
    //send userlist and chat log
    //add new fd to poll struct

    //loop through poll fds and check for events
    //user leaves
    //update userlist, send userleaves to all users
    //user joins, send userjoins to all users
    //user sends message, broadcast to all users

    //user sends chat request
    //send request to target
    //target accepts
    //establish 1 on 1
    //break 1 on 1

    return 0;
}