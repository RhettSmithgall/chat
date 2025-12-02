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

#define CHAT_CAPACITY 100
#define MSG_LEN 256

//GLOBABL VARIABLES
struct user* userlist = NULL;
struct message chatlog;

//a struct to hold users
struct user{
    char username[24]; //their username
    int fd; //the server-given fd
    int partner; //the fd of the partner
    int status; //0 online, 1 active, 2 pending, 3 private, 4 offline
    struct user* next;
};

//this is a ring buffer
//a type of fixed size array that wraps back around on itself
struct message {
    char messages[CHAT_CAPACITY][MSG_LEN];
    int start;      // oldest message index
    int count;      // how many messages are currently stored
};

void init_chatlog(struct message *log) {
    log->start = 0;
    log->count = 0;
}

void add_message(struct message *log, const char *msg) {
    int index;

    if (log->count < CHAT_CAPACITY) {
        // There is space, append at (start + count) % CHAT_CAPACITY
        index = (log->start + log->count) % CHAT_CAPACITY;
        log->count++;
    } else {
        // Full overwrite oldest
        index = log->start;
        log->start = (log->start + 1) % CHAT_CAPACITY;
    }

    strncpy(log->messages[index], msg, MSG_LEN - 1);
    log->messages[index][MSG_LEN - 1] = '\0';
}


void print_chatlog(struct message* log) {
    for (int i = 0; i < log->count; i++) {
        int index = (log->start + i) % CHAT_CAPACITY;
        printf("%s", log->messages[index]);
    }
}


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
}

char* get_username(struct user* head,int fd){
    struct user* temp = head;
    while(fd != temp->fd){
        if(temp->next == NULL){
            return NULL;
        }
        temp=temp->next;
    }
    return temp->username;
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

struct user* get_user(struct user* userlist,int fd){
    struct user* temp = userlist;
    while(temp->fd != fd){
        if(temp->next == NULL){
            return NULL;
        }
        temp=temp->next;
    }
    return temp;
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

int add_new_client(int listener,struct pollfd **pfds,int *fd_count,int *fd_size){
    struct sockaddr_storage remoteaddr;
    socklen_t addrlen = sizeof remoteaddr;

    char remoteIP[INET6_ADDRSTRLEN];

    int newfd = accept(listener, (struct sockaddr *)&remoteaddr, &addrlen);
    if (newfd == -1) {
        perror("accept");
        return -1;
    }

    char username[24];
    int nbytes = recv(newfd, username, sizeof(username), 0);
    username[nbytes] = '\0';

    printf("%s connected!\n",username);

    insertUser(&userlist,username,newfd,0,1);

    struct user* temp = userlist;
    while(temp != NULL) {
        char msg[48];
        snprintf(msg,sizeof(msg),"{%s,%d,%d,%d}",temp->username,temp->fd,temp->partner,temp->status);
        send(newfd,msg,strlen(msg),0);
        temp = temp->next;
    }
    send(newfd,"{END}",5,0);

    for (int i = 0; i < chatlog.count; i++) {
        int index = (chatlog.start + i) % CHAT_CAPACITY;
        char msg[512];
        snprintf(msg, sizeof(msg), "%s", chatlog.messages[index]);
        send(newfd, msg, strlen(msg), 0);
    }
    
    add_to_pfds(pfds, newfd, fd_count, fd_size);

    return newfd;
}

void command_handler(char buf[255], struct pollfd **pfds, int sender_fd) {
    int i = 0;

    // find the '/' command
    while (buf[i] != '/' && buf[i] != '\0') i++;
    if (buf[i] == '\0') return; // no command

    char *start = buf + i + 1;
    char *command = strtok(start, " \n");
    if (!command) return;

    struct user *temp = userlist;
    while (temp && temp->fd != sender_fd)
        temp = temp->next;

    if (!temp) return; // sender not found

    char notify[128];

    if (strcmp(command, "chat") == 0) {
        char *operand = strtok(NULL, " \n");
        if (!operand) return;

        int their_fd = atoi(operand);

        struct user *target = userlist;
        while (target && target->fd != their_fd)
            target = target->next;

        if (!target) return; // recipient not found

        if (temp->status == 1 && target->status == 1) {
            target->status = 2;
            target->partner = sender_fd;

            snprintf(notify, sizeof(notify), "{%s,%d,%d,%d}",
                    target->username,target->fd,target->partner,target->status);
            send(sender_fd, notify, strlen(notify), 0);
            send(their_fd, notify, strlen(notify), 0);
        } else {
            snprintf(notify, sizeof(notify),
                     "You can't private chat with them right now\n");
            send(sender_fd, notify, strlen(notify), 0);
        }
    }
    else if (strcmp(command, "accept") == 0) {
        if (temp->status != 2) {
            snprintf(notify, sizeof(notify),
                     "You don't have any chat invites\n");
            send(sender_fd, notify, strlen(notify), 0);
            return;
        }

        struct user *partner = userlist;
        while (partner && partner->fd != temp->partner)
            partner = partner->next;
        if (!partner) return;

        temp->status = 3;
        partner->status = 3;
        partner->partner = sender_fd;

        // notify both users
        snprintf(notify, sizeof(notify), "{%s,%d,%d,%d}",
                 temp->username, temp->fd, temp->partner, temp->status);
        send(sender_fd, notify, strlen(notify), 0);
        send(partner->fd, notify, strlen(notify), 0);

        snprintf(notify, sizeof(notify), "{%s,%d,%d,%d}",
                 partner->username, partner->fd, partner->partner, partner->status);
        send(sender_fd, notify, strlen(notify), 0);
        send(partner->fd, notify, strlen(notify), 0);
    }
    else if (strcmp(command, "deny") == 0) {
        if (temp->status != 2) return;

        struct user *partner = userlist;
        while (partner && partner->fd != temp->partner)
            partner = partner->next;
        if (!partner) return;

        temp->status = 1; // back to active
        temp->partner = 0;

        partner->status = 1;
        partner->partner = 0;

        snprintf(notify, sizeof(notify), "{%s,%d,%d,%d}",
                 temp->username, temp->fd, temp->partner, temp->status);
        send(sender_fd, notify, strlen(notify), 0);
        send(partner->fd, notify, strlen(notify), 0);
    }
    else if (strcmp(command, "exit") == 0) {
        if (temp->status != 3) return;

        struct user *partner = userlist;
        while (partner && partner->fd != temp->partner)
            partner = partner->next;
        if (!partner) return;

        temp->status = 1;
        temp->partner = 0;

        partner->status = 1;
        partner->partner = 0;

        snprintf(notify, sizeof(notify), "{%s,%d,%d,%d}",
                 temp->username, temp->fd, temp->partner, temp->status);
        send(sender_fd, notify, strlen(notify), 0);
        send(partner->fd, notify, strlen(notify), 0);

        snprintf(notify, sizeof(notify), "{%s,%d,%d,%d}",
                 partner->username, partner->fd, partner->partner, partner->status);
        send(sender_fd, notify, strlen(notify), 0);
        send(partner->fd, notify, strlen(notify), 0);
    }
}

int main(int argc, char* argv[]){
    int listener;
    struct sockaddr_in my_addr;

    init_chatlog(&chatlog);

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
                    int newfd = add_new_client(listener, &pfds, &fd_count, &fd_size);
                    struct user* temp = get_user(userlist,newfd);

                    char notify[128];
                    snprintf(notify, sizeof(notify),"{%s,%d,%d,%d}",
                        temp->username,temp->fd,temp->partner,4);
                    snprintf(buf, sizeof(buf),"%s connected\n", temp->username);

                    for (int j = 0; j < fd_count; j++) {
                        int dest_fd = pfds[j].fd;

                        if (dest_fd != listener && dest_fd != newfd) {
                            send(dest_fd, notify, strlen(notify), 0);
                            send(dest_fd, buf, strlen(buf), 0);
                        }
                    }

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
                        snprintf(notify, sizeof(notify),"{%s,%d,%d,%d}",
                            temp->username,temp->fd,temp->partner,4);
                        snprintf(buf, sizeof(buf),"%s disconnected\n", temp->username);

                        for (int j = 0; j < fd_count; j++) {
                            int dest_fd = pfds[j].fd;

                            if (dest_fd != listener && dest_fd != sender_fd) {
                                send(dest_fd, notify, strlen(notify), 0);
                                send(dest_fd, buf, strlen(buf), 0);
                            }
                        }

                        if(temp->partner != 0){
                            struct user* curr = get_user(userlist,temp->partner);
                            curr->status = 1;
                            curr->partner = 0;
                        }
                    }

                    // Remove from poll list
                    removeUser(userlist,sender_fd);
                    close(sender_fd);
                    del_from_pfds(pfds, i, &fd_count);
                    i--;   // IMPORTANT: adjust index because pfds shifted
                }
                else {
                    // -------------------------
                    // NORMAL MESSAGE RECV
                    // -------------------------
                    // Broadcast buf to all others

                    //if its a private chat request we need to send that to the partner
                    buf[nbytes] = '\0';
                    int found = 0;
                    for(int i=0;i<nbytes;i++){
                        if(buf[i] == '/'){
                            found = 1;
                        }
                    }
                    
                    if(found == 1){
                        command_handler(buf,&pfds,sender_fd);
                    }
                    else{ //no command, broadcast as normal
                        char msg[256];
                        snprintf(msg, sizeof(msg),"%s:%s",get_username(userlist,sender_fd),buf);
                        add_message(&chatlog,msg);

                        struct user* sender = get_user(userlist,sender_fd);
                        if(sender->status != 3){ 
                            for (int j = 0; j < fd_count; j++) {
                                int dest_fd = pfds[j].fd;

                                struct user* reciever = get_user(userlist,dest_fd);

                                if(reciever->partner == 0){
                                    if (dest_fd != listener && dest_fd != sender_fd) {
                                        if (send(dest_fd, msg, strlen(msg), 0) == -1) {
                                            perror("send");
                                        }
                                    }
                                }
                            }
                        }
                        else{
                            if (send(sender->partner, msg, strlen(msg), 0) == -1) {
                                perror("send");
                            }
                        }
                    }
                }
            }
        }
    }
    return 0;
}
