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

#define SERVER_HOSTNAME "zos.ospreys.biz"
#define SERVER_PORT "50074"

#define CHAT_CAPACITY 100
#define MSG_LEN 256

// User status codes
typedef enum {
    ONLINE =  0,
    ACTIVE =  1,
    PENDING = 2,
    PRIVATE = 3,
    OFFLINE = 4
    // offline / other statuses handled elsewhere
} UserStatus;

//a struct to hold users
struct user{
    char username[24]; //their username
    int fd; //the server-given fd
    int partner; //the fd of the partner
    int status; //-1 offline, 0 online, 1 active, 2 pending, 3 private
    struct user* next;
};

struct message {
    char messages[CHAT_CAPACITY][MSG_LEN];
    int start;      // oldest message index
    int count;      // how many messages are currently stored
};

//GLOBAL VARIABLES
struct user* userlist = NULL;
struct message chatlog;
int myFd; //the fd the server has assigned you

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
        if(temp->status == 1 || temp->status == 2){
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
void *get_in_addr(struct sockaddr *sa){
    if (sa->sa_family == AF_INET) {
        return &(((struct sockaddr_in*)sa)->sin_addr);
    }

    return &(((struct sockaddr_in6*)sa)->sin6_addr);
}

//connects to server then returns the server fd
int connect_to_server(const char *host, const char *port){
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

void parse_user(const char buffer[32]) {
    char username[12];
    int fd, partner, status;
    int matched = sscanf(buffer, "%11[^,],%d,%d,%d", 
                         username, &fd, &partner, &status);
    if (matched == 4) {
        insertUser(&userlist, username, fd, partner, status);
    }
}

void change_user(char buffer[48]) {
    char username[12];
    int fd, partner, status;

    // Parse the server message safely
    int matched = sscanf(buffer, "{%11[^,],%d,%d,%d}", username, &fd, &partner, &status);
    username[11] = '\0'; // ensure null termination

    printf("[CHANGE_USER] matched=%d user=%s fd=%d partner=%d status=%d\n",
           matched, username, fd, partner, status);

    if (matched != 4) return;

    // If the update involves this client
    if (fd == myFd) {
        switch (status) {
            case PRIVATE:
                printf("BEGIN PRIVATE CHAT\n");
                return;
            case PENDING:
                printf("CHAT REQUEST FROM %s\n", get_username(userlist,partner));
                return;
            case ACTIVE:
                printf("END PRIVATE CHAT\n");
                return;
            default:
                // ignore other statuses for self
                return;
        }
    }

    struct user* temp = userlist;
    while(temp->fd != myFd){
        if(temp->next == NULL){
            insertUser(&userlist,username,fd,partner,status);
            return;
        }
        temp=temp->next;
    }

    if(fd == temp->partner){
        printf("PARTNER DISCONNECTED!\n");
        temp->partner = 0;
        temp->status = 1;
    }

    // Update or remove other users
    if (status > PRIVATE) {
        removeUser(userlist, fd);
    } else {
        updateUser(userlist, fd, username, partner, status);
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

void recv_loop(int sockfd) {
    char c;
    char buffer[32];
    int buf_idx = 0;
    int in_packet = 0;
    ssize_t n;
    
    while (1) {
        n = recv(sockfd, &c, 1, 0);
        
        if (n <= 0) {
            if (n == 0) {
                printf("Connection closed\n");
            } else {
                perror("recv error");
            }
            break;
        }
        
        if (c == '{') {
            in_packet = 1;
            buf_idx = 0;
            buffer[buf_idx++] = c;
        } else if (in_packet) {
            if (buf_idx < 31) {
                buffer[buf_idx++] = c;
            }
            
            if (c == '}') {
                buffer[buf_idx] = '\0';
                change_user(buffer);
                in_packet = 0;
                buf_idx = 0;
            }
        } else {
            putchar(c);
            fflush(stdout);
        }
    }
}

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/*
 * cut_substring:
 *    src   = original null-terminated string
 *    start = index where substring begins
 *    len   = length of substring to cut
 *
 * Returns:
 *    *out_cut = newly allocated substring
 *    *out_remaining = newly allocated string with the substring removed
 *
 * Caller must free both outputs.
 */
void cut_substring(const char *src, size_t start, size_t len,
                   char **out_cut, char **out_remaining){
    size_t src_len = strlen(src);

    // Bounds guard
    if (start > src_len) start = src_len;
    if (start + len > src_len) len = src_len - start;

    // Allocate substring
    *out_cut = malloc(len + 1);
    if (!*out_cut) {
        perror("malloc");
        exit(1);
    }
    memcpy(*out_cut, src + start, len);
    (*out_cut)[len] = '\0';

    // Allocate remaining string
    size_t remaining_len = src_len - len;
    *out_remaining = malloc(remaining_len + 1);
    if (!*out_remaining) {
        perror("malloc");
        exit(1);
    }

    // Copy the two parts around the cut-out region
    memcpy(*out_remaining, src, start);                      // left part
    memcpy(*out_remaining + start, src + start + len,        // right part
           src_len - (start + len));

    (*out_remaining)[remaining_len] = '\0';
}


int main(int argc,char* argv[]){
    init_chatlog(&chatlog);

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

    struct pollfd fds[2];
    fds[0].fd = STDIN_FILENO;
    fds[0].events = POLLIN;

    fds[1].fd = server_fd;
    fds[1].events = POLLIN;

    char buf[256];
    char c;

    for (;;) {
        int rv = poll(fds, 2, -1);
        if (rv < 0) {
            perror("poll");
            exit(1);
        }

        // ---- stdin -> server ----
        if (fds[0].revents & POLLIN) {
            ssize_t len = read(STDIN_FILENO, buf, sizeof(buf));
            if (len <= 0) {
                printf("stdin closed\n");
                break;
            }

            // Send EXACTLY len bytes
            ssize_t sent = send(server_fd, buf, len, 0);
            if (sent <= 0) {
                perror("send");
                break;
            }
        }

        // ---- server -> client ----
        if (fds[1].revents & POLLIN) {
            ssize_t len = recv(server_fd, buf, sizeof(buf) - 1, 0);

            if (len == 0) {
                printf("server disconnected\n");
                break;
            }
            if (len < 0) {
                perror("recv");
                break;
            }
            buf[len] = '\0';

            for(int i=0;i<len;i++){
                if(buf[i] == '{'){
                    for(int j=i;j<len;j++){
                        if(buf[j] == '}'){ //yay! the whole packet came through!
                            char *sub,*remaining;
                            cut_substring(buf, i, j + 1, &sub, &remaining);
                            printf("%s\n",sub);
                            change_user(sub);
                            strcpy(buf,remaining);
                            buf[strlen(remaining)];
                            free(sub);
                            free(remaining);
                        }
                    }
                }
            }

            //no brackets!
            printf("%s", buf);
            fflush(stdout);
            add_message(&chatlog,buf);
        }
    }
    return 0;
    
}