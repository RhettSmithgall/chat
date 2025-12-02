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

#define SERVER_HOSTNAME "systems.ospreys.biz" //zos.ospreys.biz
#define SERVER_PORT "1112" //50074
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

struct user* findUserByFd(int fd) {
    struct user* temp = userlist;
    while (temp != NULL) {
        if (temp->fd == fd) return temp;
        temp = temp->next;
    }
    return NULL;
}

void updateUserStatus(int fd, int partner, int status) {
    struct user* user = findUserByFd(fd);
    if (user) {
        user->partner = partner;
        user->status = status;
        printf("Updated %s: status=%d, partner=%d\n", user->username, status, partner);
    }
}

void removeUser(int fd) {
    struct user* temp = userlist;
    struct user* prev = NULL;
    
    if (temp == NULL) return;
    
    // If head needs to be removed
    if (temp->fd == fd) {
        userlist = temp->next;
        printf("Removed user: %s\n", temp->username);
        free(temp);
        return;
    }
    
    while (temp != NULL && temp->fd != fd) {
        prev = temp;
        temp = temp->next;
    }
    
    if (temp == NULL) return;
    
    prev->next = temp->next;
    printf("Removed user: %s\n", temp->username);
    free(temp);
}

void broadcastUserUpdate(struct user* user, int listener, struct pollfd* pfds, int fd_count) {
    if (!user) return;
    
    char notify[128];
    snprintf(notify, sizeof(notify), "{%s,%d,%d,%d}", 
             user->username, user->fd, user->partner, user->status);
    
    for (int j = 0; j < fd_count; j++) {
        int dest_fd = pfds[j].fd;
        if (dest_fd != listener && dest_fd != user->fd) {
            send(dest_fd, notify, strlen(notify), 0);
        }
    }
}

// Add a new file descriptor to the set
void add_to_pfds(struct pollfd *pfds[], int newfd, int *fd_count, int *fd_size){
    if (*fd_count == *fd_size) {
        *fd_size *= 2;
        *pfds = realloc(*pfds, sizeof(**pfds) * (*fd_size));
    }

    (*pfds)[*fd_count].fd = newfd;
    (*pfds)[*fd_count].events = POLLIN; // Check ready-to-read

    (*fd_count)++;
}

void add_new_client(int listener,struct pollfd **pfds,int *fd_count,int *fd_size){
    struct sockaddr_storage remoteaddr;
    socklen_t addrlen = sizeof remoteaddr;

    // char remoteIP[INET6_ADDRSTRLEN];

    int newfd = accept(listener, (struct sockaddr *)&remoteaddr, &addrlen);
    if (newfd == -1) {
        perror("accept");
        return;
    }

    char username[24];
    int nbytes = recv(newfd, username, sizeof(username), 0);
    username[nbytes] = '\0';

    printf("%s connected!\n", username);

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

    char newUserMsg[64];
    snprintf(newUserMsg, sizeof(newUserMsg), "{%s,%d,%d,%d}", username, newfd, 0, 1);
    for (int i = 0; i < *fd_count; i++) {
        int destFd = (*pfds)[i].fd;
        if (destFd != listener && destFd != newfd) {
            send(destFd, newUserMsg, strlen(newUserMsg), 0);
        }
    }

    add_to_pfds(pfds, newfd, fd_count, fd_size);
}

int main(int argc, char* argv[]){
    int listener;
    // struct sockaddr_in my_addr;

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
    // insertUser(&userlist, "s990333", 20, 0 , 0);
    // insertUser(&userlist, "s990355", 40, 0 , 0);
    // insertUser(&userlist, "s990367", 45, 0 , 0);
    // insertUser(&userlist, "s990101", 10, 15, 3);
    // insertUser(&userlist, "s990123", 15, 10, 3);
    // insertUser(&userlist, "s990444", 25, 0 , 1);
    // insertUser(&userlist, "s990555", 30, 0 , 1);
    // insertUser(&userlist, "s990666", 35, 0 , 1);

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
                        // Broadcast user disconnect to everyone (status -1 = offline)
                        char notify[128];
                        snprintf(notify, sizeof(notify), "{%s,%d,%d,%d}",
                            temp->username, temp->fd, temp->partner, -1);

                        for (int j = 0; j < fd_count; j++) {
                            int dest_fd = pfds[j].fd;

                            if (dest_fd != listener && dest_fd != sender_fd) {
                                send(dest_fd, notify, strlen(notify), 0);
                            }
                        }
                        
                        // Remove user from userlist
                        removeUser(sender_fd);
                    }

                    // Remove from poll list
                    close(sender_fd);
                    del_from_pfds(pfds, i, &fd_count);
                    i--;   // IMPORTANT: adjust index because pfds shifted
                }
                else {
                    buf[nbytes] = '\0';
                    struct user *sender = findUserByFd(sender_fd);
                    
                    // Check for commands
                    if (strncmp(buf, "/request ", 9) == 0) {
                        // Client wants to request private chat
                        int targetFd = atoi(buf + 9);
                        struct user *target = findUserByFd(targetFd);
                        
                        if (sender && target && target->status == 1) {
                            // Set sender to pending (status=2), store who they requested
                            updateUserStatus(sender_fd, targetFd, 2);
                            
                            // Notify target of the request
                            char notify[128];
                            snprintf(notify, sizeof(notify), "/chatrequest %d %s", sender_fd, sender->username);
                            send(targetFd, notify, strlen(notify), 0);
                            
                            printf("%s requested private chat with %s\n", sender->username, target->username);
                        }
                        continue;
                    }
                    
                    if (strncmp(buf, "/accept ", 8) == 0) {
                        // Client accepts a chat request
                        int requesterFd = atoi(buf + 8);
                        struct user *requester = findUserByFd(requesterFd);
                        
                        if (sender && requester && requester->status == 2 && requester->partner == sender_fd) {
                            // Both users enter private chat
                            updateUserStatus(sender_fd, requesterFd, 3);
                            updateUserStatus(requesterFd, sender_fd, 3);
                            
                            // Broadcast status changes to all other users
                            broadcastUserUpdate(sender, listener, pfds, fd_count);
                            broadcastUserUpdate(requester, listener, pfds, fd_count);
                            
                            // Notify requester that request was accepted
                            char notify[128];
                            snprintf(notify, sizeof(notify), "/accepted %d %s", sender_fd, sender->username);
                            send(requesterFd, notify, strlen(notify), 0);
                            
                            printf("%s accepted chat from %s\n", sender->username, requester->username);
                        }
                        continue;
                    }
                    
                    if (strncmp(buf, "/decline ", 9) == 0) {
                        // Client declines a chat request
                        int requesterFd = atoi(buf + 9);
                        struct user *requester = findUserByFd(requesterFd);
                        
                        if (requester && requester->status == 2 && requester->partner == sender_fd) {
                            // Return requester to lobby
                            updateUserStatus(requesterFd, 0, 1);
                            
                            // Notify requester that request was declined
                            char notify[64];
                            snprintf(notify, sizeof(notify), "/declined %s", sender->username);
                            send(requesterFd, notify, strlen(notify), 0);
                            
                            printf("%s declined chat from %s\n", sender->username, requester->username);
                        }
                        continue;
                    }
                    
                    if (strncmp(buf, "/lobby", 6) == 0) {
                        // Client wants to return to lobby
                        int partnerFd = sender ? sender->partner : 0;
                        
                        if (sender && sender->status == 3 && partnerFd > 0) {
                            // Notify partner that chat ended
                            char notify[64];
                            snprintf(notify, sizeof(notify), "/partleft %s", sender->username);
                            send(partnerFd, notify, strlen(notify), 0);
                            
                            // Return partner to lobby too
                            updateUserStatus(partnerFd, 0, 1);
                        }
                        
                        // Update sender status
                        updateUserStatus(sender_fd, 0, 1);
                        
                        // Broadcast both users return to lobby after both are updated
                        if (partnerFd > 0) {
                            struct user *partner = findUserByFd(partnerFd);
                            if (partner) {
                                broadcastUserUpdate(partner, listener, pfds, fd_count);
                            }
                        }
                        broadcastUserUpdate(sender, listener, pfds, fd_count);
                        continue;
                    }
                    
                    // Handle regular messages based on status
                    if (sender && sender->status == 3) {
                        // PRIVATE CHAT: Only send to partner
                        if (sender->partner > 0) {
                            if (send(sender->partner, buf, nbytes, 0) == -1) {
                                perror("send private");
                            }
                        }
                    } else if (sender && sender->status == 1) {
                        // LOBBY CHAT: Send to all users in lobby (status 1)
                        for (int j = 0; j < fd_count; j++) {
                            int dest_fd = pfds[j].fd;

                            if (dest_fd != listener && dest_fd != sender_fd) {
                                struct user *dest = findUserByFd(dest_fd);
                                if (dest && dest->status == 1) {
                                    if (send(dest_fd, buf, nbytes, 0) == -1) {
                                        perror("send lobby");
                                    }
                                }
                            }
                        }
                    }
                    // If status == 2 (pending), don't send messages anywhere
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
