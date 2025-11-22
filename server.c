#include "headers.h"

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
    server_addr.sin_addr.s_addr = INADDR_ANY;     // ip address
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

    // pollfd array: slot 0 = listener, 1âMAX_CLIENTS = clients
    struct pollfd fds[BACKLOG + 1];
    fds[0].fd = server_fd;
    fds[0].events = POLLIN;

    // Initialize other slots as empty
    for (int i = 1; i <= BACKLOG; i++) {
        fds[i].fd = -1;
        fds[i].events = POLLIN;
    }

    int new_fd;
    addr_size = sizeof(their_addr);

        while (1) {
        int ret = poll(fds, BACKLOG + 1, -1);
        if (ret < 0) {
            perror("poll");
            continue;
        }

        // -------------------------
        // New connection
        // -------------------------
        if (fds[0].revents & POLLIN) {
            int clientfd = accept(server_fd, NULL, NULL);
            if (clientfd < 0) {
                perror("accept");
                continue;
            }

            // Full?
            if (count_clients() >= BACKLOG) {
                const char *msg = "SERVER FULL\n";
                send(clientfd, msg, strlen(msg), 0);
                close(clientfd);
                printf("Rejected connection: server full\n");
                continue;
            }

            // Find empty poll slot
            int placed = 0;
            for (int i = 1; i <= BACKLOG; i++) {
                if (fds[i].fd == -1) {
                    fds[i].fd = clientfd;
                    placed = 1;
                    break;
                }
            }

            // Should never happen here, but safe
            if (!placed) {
                const char *msg = "SERVER FULL\n";
                send(clientfd, msg, strlen(msg), 0);
                close(clientfd);
                continue;
            }

            add_client(clientfd);
            printf("Client connected (fd=%d)\n", clientfd);
        }

        // -------------------------
        // Existing clients
        // -------------------------
        for (int i = 1; i <= BACKLOG; i++) {
            int fd = fds[i].fd;
            if (fd == -1) continue;

            if (fds[i].revents & (POLLIN | POLLERR | POLLHUP)) {
                char buf[BACKLOG];
                int n = recv(fd, buf, sizeof(buf)-1, 0);

                // Client disconnected
                if (n <= 0) {
                    printf("Client disconnected (fd=%d)\n", fd);
                    close(fd);
                    remove_client(fd);
                    fds[i].fd = -1;
                    continue;
                }

                buf[n] = '\0';
                printf("Message from %d: %s", fd, buf);

                // send message to all other clients
                for (int j = 1; j <= BACKLOG; j++) {
                    int other_fd = fds[j].fd;
                    if (other_fd != -1 && other_fd != fd) {
                        send(other_fd, buf, n, 0);
                        
                    }
                }
            }
        }
    }

    return 0;
}
