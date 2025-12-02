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
#include <ncurses.h>

#define SERVER_HOSTNAME "localhost"
#define SERVER_PORT "1112"

#define MAX_USERS 100
#define MAX_MESSAGES 100
#define MAX_INPUT 256

// View states
typedef enum {
    VIEW_LOBBY,
    VIEW_PRIVATE
} ViewState;

typedef struct User {
    char username[24];
    int fd;
    int partner;
    int status;  // -1 offline, 0 online, 1 active, 2 pending, 3 private
    struct User *next;
} User;

// Panel struct for ncurses windows
typedef struct {
    WINDOW *win;
    int height;
    int width;
    int y;
    int x;
} Panel;

// Global state
ViewState currentView = VIEW_LOBBY;
User *userlist = NULL;
int myFd = -1;
int serverFd = -1;
char myUsername[24] = {0};

// Chat messages
char *lobbyMessages[MAX_MESSAGES];
int lobbyMsgCount = 0;
char *privateMessages[MAX_MESSAGES];
int privateMsgCount = 0;

// Input
char inputBuffer[MAX_INPUT] = {0};
int inputPos = 0;

// Lobby panels
Panel leftPanels[3];  // Active, Online, Private
Panel chatPanel;
Panel inputPanel;
int activePanel = 0;  // 0=active, 1=online, 2=private, 3=input

// Private chat panels
Panel partnerPanel;
Panel myPanel;
Panel privateInputPanel;
char partnerName[24] = {0};
int partnerFd = -1;  // Track partner's fd for server communication

// Pending chat request tracking
int pendingRequestFd = -1;  // fd of user who sent us a chat request
char pendingRequestName[24] = {0};

// Selection tracking for user lists
int activeSelected = 0, onlineSelected = 0, privateSelected = 0;
int activeScroll = 0, onlineScroll = 0, privateScroll = 0;

// Screen dimensions
int maxY, maxX;


// NETWORKING FUNCTIONS
void *getInAddr(struct sockaddr *sa) {
    if (sa->sa_family == AF_INET) {
        return &(((struct sockaddr_in*)sa)->sin_addr);
    }
    return &(((struct sockaddr_in6*)sa)->sin6_addr);
}

int connectToServer(const char *host, const char *port) {
    struct addrinfo hints, *servinfo, *p;
    int rv, sockfd = -1;

    memset(&hints, 0, sizeof hints);
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;

    if ((rv = getaddrinfo(host, port, &hints, &servinfo)) != 0) {
        return -1;
    }

    for (p = servinfo; p != NULL; p = p->ai_next) {
        sockfd = socket(p->ai_family, p->ai_socktype, p->ai_protocol);
        if (sockfd == -1) continue;

        if (connect(sockfd, p->ai_addr, p->ai_addrlen) == -1) {
            close(sockfd);
            sockfd = -1;
            continue;
        }
        break;
    }

    freeaddrinfo(servinfo);
    return sockfd;
}

// USER LIST FUNCTIONS
User *createUser(const char *username, int fd, int partner, int status) {
    User *newNode = (User*)malloc(sizeof(User));
    strncpy(newNode->username, username, sizeof(newNode->username) - 1);
    newNode->username[sizeof(newNode->username) - 1] = '\0';
    newNode->fd = fd;
    newNode->partner = partner;
    newNode->status = status;
    newNode->next = NULL;
    return newNode;
}

User *insertUser(User **head, const char *username, int fd, int partner, int status) {
    User *newUser = createUser(username, fd, partner, status);
    if (*head == NULL) {
        *head = newUser;
        return newUser;
    }
    User *temp = *head;
    while (temp->next != NULL) {
        temp = temp->next;
    }
    temp->next = newUser;
    return newUser;
}

int updateUser(User *head, int fd, const char *username, int partner, int status) {
    User *curr = head;
    while (curr != NULL) {
        if (curr->fd == fd) {
            strncpy(curr->username, username, sizeof(curr->username) - 1);
            curr->username[sizeof(curr->username) - 1] = '\0';
            curr->partner = partner;
            curr->status = status;
            return 1;
        }
        curr = curr->next;
    }
    return 0;
}

void removeUser(User **head, int fd) {
    User *temp = *head;
    User *prev = NULL;

    if (temp == NULL) return;

    // If head needs to be removed
    if (temp->fd == fd) {
        *head = temp->next;
        free(temp);
        return;
    }

    while (temp != NULL && temp->fd != fd) {
        prev = temp;
        temp = temp->next;
    }

    if (temp == NULL) return;

    prev->next = temp->next;
    free(temp);
}

User *findUserByFd(User *head, int fd) {
    while (head != NULL) {
        if (head->fd == fd) return head;
        head = head->next;
    }
    return NULL;
}

// Count users by status
int countUsersByStatus(User *head, int status) {
    int count = 0;
    while (head != NULL) {
        if (head->status == status) count++;
        head = head->next;
    }
    return count;
}

// Get user at index with specific status
User *getUserByStatusIndex(User *head, int status, int index) {
    int count = 0;
    while (head != NULL) {
        if (head->status == status) {
            if (count == index) return head;
            count++;
        }
        head = head->next;
    }
    return NULL;
}

// MESSAGE FUNCTIONS
void addLobbyMessage(const char *msg) {
    if (lobbyMsgCount >= MAX_MESSAGES) {
        free(lobbyMessages[0]);
        for (int i = 1; i < MAX_MESSAGES; i++) {
            lobbyMessages[i-1] = lobbyMessages[i];
        }
        lobbyMsgCount--;
    }
    lobbyMessages[lobbyMsgCount++] = strdup(msg);
}

void addPrivateMessage(const char *msg) {
    if (privateMsgCount >= MAX_MESSAGES) {
        free(privateMessages[0]);
        for (int i = 1; i < MAX_MESSAGES; i++) {
            privateMessages[i-1] = privateMessages[i];
        }
        privateMsgCount--;
    }
    privateMessages[privateMsgCount++] = strdup(msg);
}

// PROTOCOL PARSING
void parseUser(const char *buffer) {
    char username[24];
    int fd, partner, status;
    int matched = sscanf(buffer, "%23[^,],%d,%d,%d", username, &fd, &partner, &status);
    if (matched == 4) {
        insertUser(&userlist, username, fd, partner, status);
    }
}

void changeUser(const char *buffer) {
    char username[24];
    int fd, partner, status;
    int matched = sscanf(buffer, "{%23[^,],%d,%d,%d}", username, &fd, &partner, &status);
    if (matched == 4) {
        if (status < 0) {
            // User disconnected
            User *leaving = findUserByFd(userlist, fd);
            if (leaving) {
                char msg[64];
                snprintf(msg, sizeof(msg), "*** %s has left ***", leaving->username);
                addLobbyMessage(msg);
            }
            removeUser(&userlist, fd);
        } else {
            // Check if user already exists
            User *existing = findUserByFd(userlist, fd);
            if (existing) {
                updateUser(userlist, fd, username, partner, status);
            } else {
                // New user joined
                insertUser(&userlist, username, fd, partner, status);
                char msg[64];
                snprintf(msg, sizeof(msg), "*** %s has joined ***", username);
                addLobbyMessage(msg);
            }
        }
    }
}

void getUserlist(int fd) {
    char buf[32];
    char c;

    while (1) {
        recv(fd, &c, 1, 0);
        while (c == '{') {
            int i = 0;
            memset(buf, 0, sizeof(buf));
            recv(fd, &c, 1, 0);
            while (c != '}' && i < 31) {
                buf[i++] = c;
                recv(fd, &c, 1, 0);
            }
            if (strcmp(buf, "END") == 0) {
                // Find our fd
                User *temp = userlist;
                while (temp && temp->next != NULL) {
                    temp = temp->next;
                }
                if (temp) myFd = temp->fd;
                return;
            } else {
                parseUser(buf);
                recv(fd, &c, 1, 0);
            }
        }
    }
}

// LOBBY UI FUNCTIONS
void drawBoxWithTitle(WINDOW *win, const char *title, int isActive) {
    box(win, 0, 0);
    if (isActive) {
        wattron(win, A_BOLD | COLOR_PAIR(1));
    }
    mvwprintw(win, 0, 2, " %s ", title);
    if (isActive) {
        wattroff(win, A_BOLD | COLOR_PAIR(1));
    }
}

void renderUserPanel(Panel *panel, int status, const char *title, int isActive, int *selected, int *scroll) {
    werase(panel->win);
    drawBoxWithTitle(panel->win, title, isActive);

    int displayHeight = panel->height - 2;
    int count = countUsersByStatus(userlist, status);

    // Clamp selection
    if (*selected >= count) *selected = count > 0 ? count - 1 : 0;
    if (*selected < 0) *selected = 0;

    // Adjust scroll
    if (*selected < *scroll) {
        *scroll = *selected;
    } else if (*selected >= *scroll + displayHeight) {
        *scroll = *selected - displayHeight + 1;
    }

    int idx = 0;
    User *temp = userlist;
    int line = 1;
    while (temp != NULL && line <= displayHeight) {
        if (temp->status == status) {
            if (idx >= *scroll && idx < *scroll + displayHeight) {
                if (idx == *selected && isActive) {
                    wattron(panel->win, A_REVERSE);
                }
                // Highlight self in yellow
                if (temp->fd == myFd) {
                    wattron(panel->win, COLOR_PAIR(3));
                }
                mvwprintw(panel->win, line, 2, "%s", temp->username);
                if (temp->fd == myFd) {
                    wattroff(panel->win, COLOR_PAIR(3));
                }
                if (idx == *selected && isActive) {
                    wattroff(panel->win, A_REVERSE);
                }
                line++;
            }
            idx++;
        }
        temp = temp->next;
    }

    wrefresh(panel->win);
}

void renderChatPanel() {
    werase(chatPanel.win);
    drawBoxWithTitle(chatPanel.win, "Chat", 0);

    int displayHeight = chatPanel.height - 2;
    int start = (lobbyMsgCount > displayHeight) ? lobbyMsgCount - displayHeight : 0;

    for (int i = start; i < lobbyMsgCount; i++) {
        int line = i - start + 1;
        mvwprintw(chatPanel.win, line, 2, "%.*s", chatPanel.width - 4, lobbyMessages[i]);
    }

    wrefresh(chatPanel.win);
}

void renderInputPanel() {
    werase(inputPanel.win);
    drawBoxWithTitle(inputPanel.win, "Message (TAB=switch, ESC=quit)", activePanel == 3);
    mvwprintw(inputPanel.win, 1, 2, "> %s", inputBuffer);
    wrefresh(inputPanel.win);
}

void renderLobby() {
    renderUserPanel(&leftPanels[0], 1, "Active Users", activePanel == 0, &activeSelected, &activeScroll);
    renderUserPanel(&leftPanels[1], 0, "Online Users", activePanel == 1, &onlineSelected, &onlineScroll);
    renderUserPanel(&leftPanels[2], 3, "Private Chats", activePanel == 2, &privateSelected, &privateScroll);
    renderChatPanel();
    renderInputPanel();
}

// PRIVATE CHAT UI FUNCTIONS
void renderPrivateChat() {
    // Partner panel (top)
    werase(partnerPanel.win);
    drawBoxWithTitle(partnerPanel.win, partnerName, 0);
    
    int displayHeight = partnerPanel.height - 2;
    int partnerMsgCount = 0;
    
    for (int i = 0; i < privateMsgCount; i++) {
        if (strncmp(privateMessages[i], "You:", 4) != 0) {
            partnerMsgCount++;
        }
    }
    
    int line = 1;
    for (int i = 0; i < privateMsgCount && line <= displayHeight; i++) {
        if (strncmp(privateMessages[i], "You:", 4) != 0) {
            mvwprintw(partnerPanel.win, line++, 2, "%.*s", partnerPanel.width - 4, privateMessages[i]);
        }
    }
    wrefresh(partnerPanel.win);

    werase(myPanel.win);
    drawBoxWithTitle(myPanel.win, "You", 0);
    
    line = 1;
    for (int i = 0; i < privateMsgCount && line <= displayHeight; i++) {
        if (strncmp(privateMessages[i], "You:", 4) == 0) {
            mvwprintw(myPanel.win, line++, 2, "%.*s", myPanel.width - 4, privateMessages[i]);
        }
    }
    wrefresh(myPanel.win);

    // Input panel (bottom)
    werase(privateInputPanel.win);
    drawBoxWithTitle(privateInputPanel.win, "Message (/leave to exit, ESC=quit)", 1);
    mvwprintw(privateInputPanel.win, 1, 2, "> %s", inputBuffer);
    wrefresh(privateInputPanel.win);
}

// WINDOW INITIALIZATION
void initLobbyWindows() {
    int leftWidth = maxX / 4;
    int rightWidth = maxX - leftWidth;
    int leftPanelHeight = maxY / 3;

    // Create left panels (user lists)
    for (int i = 0; i < 3; i++) {
        leftPanels[i].height = leftPanelHeight;
        leftPanels[i].width = leftWidth;
        leftPanels[i].y = i * leftPanelHeight;
        leftPanels[i].x = 0;
        leftPanels[i].win = newwin(leftPanelHeight, leftWidth, leftPanels[i].y, leftPanels[i].x);
    }

    // Chat panel
    chatPanel.height = maxY - 4;
    chatPanel.width = rightWidth;
    chatPanel.y = 0;
    chatPanel.x = leftWidth;
    chatPanel.win = newwin(chatPanel.height, chatPanel.width, chatPanel.y, chatPanel.x);

    // Input panel
    inputPanel.height = 4;
    inputPanel.width = rightWidth;
    inputPanel.y = maxY - 4;
    inputPanel.x = leftWidth;
    inputPanel.win = newwin(inputPanel.height, inputPanel.width, inputPanel.y, inputPanel.x);
}

void initPrivateWindows() {
    int chatHeight = (maxY - 4) / 2;

    // Partner panel (top)
    partnerPanel.height = chatHeight;
    partnerPanel.width = maxX;
    partnerPanel.y = 0;
    partnerPanel.x = 0;
    partnerPanel.win = newwin(chatHeight, maxX, 0, 0);

    // My panel (middle)
    myPanel.height = chatHeight;
    myPanel.width = maxX;
    myPanel.y = chatHeight;
    myPanel.x = 0;
    myPanel.win = newwin(chatHeight, maxX, chatHeight, 0);

    // Input panel (bottom)
    privateInputPanel.height = 4;
    privateInputPanel.width = maxX;
    privateInputPanel.y = maxY - 4;
    privateInputPanel.x = 0;
    privateInputPanel.win = newwin(4, maxX, maxY - 4, 0);
}

void destroyLobbyWindows() {
    for (int i = 0; i < 3; i++) {
        if (leftPanels[i].win) delwin(leftPanels[i].win);
    }
    if (chatPanel.win) delwin(chatPanel.win);
    if (inputPanel.win) delwin(inputPanel.win);
}

void destroyPrivateWindows() {
    if (partnerPanel.win) delwin(partnerPanel.win);
    if (myPanel.win) delwin(myPanel.win);
    if (privateInputPanel.win) delwin(privateInputPanel.win);
}

void switchToPrivateView(const char *partner, int pFd) {
    strncpy(partnerName, partner, sizeof(partnerName) - 1);
    partnerFd = pFd;
    
    destroyLobbyWindows();
    clear();
    refresh();
    initPrivateWindows();
    currentView = VIEW_PRIVATE;
    
    // Clear private messages
    for (int i = 0; i < privateMsgCount; i++) {
        free(privateMessages[i]);
    }
    privateMsgCount = 0;
    
    char msg[64];
    snprintf(msg, sizeof(msg), "*** Private chat with %s started ***", partner);
    addPrivateMessage(msg);
    addPrivateMessage("*** Type /leave to return to lobby ***");
}

void switchToLobbyViewLocal() {
    partnerFd = -1;
    memset(partnerName, 0, sizeof(partnerName));
    
    destroyPrivateWindows();
    clear();
    refresh();
    initLobbyWindows();
    currentView = VIEW_LOBBY;
    activePanel = 3;
    
    addLobbyMessage("*** Returned to lobby ***");
}

void switchToLobbyView() {
    send(serverFd, "/lobby", 6, 0);
    switchToLobbyViewLocal();
}

// INPUT HANDLING
void handleLobbyEnter() {
    if (activePanel == 3) {
        // Send message to chat
        if (inputPos > 0) {
            inputBuffer[inputPos] = '\0';
            
            // Check for /accept command
            if (strcmp(inputBuffer, "/accept") == 0) {
                if (pendingRequestFd > 0) {
                    char cmd[64];
                    snprintf(cmd, sizeof(cmd), "/accept %d", pendingRequestFd);
                    send(serverFd, cmd, strlen(cmd), 0);
                    
                    // Switch to private view with the requester
                    switchToPrivateView(pendingRequestName, pendingRequestFd);
                    
                    pendingRequestFd = -1;
                    memset(pendingRequestName, 0, sizeof(pendingRequestName));
                } else {
                    addLobbyMessage("*** No pending chat request ***");
                }
                memset(inputBuffer, 0, sizeof(inputBuffer));
                inputPos = 0;
                return;
            }
            
            // Check for /decline command
            if (strcmp(inputBuffer, "/decline") == 0) {
                if (pendingRequestFd > 0) {
                    char cmd[64];
                    snprintf(cmd, sizeof(cmd), "/decline %d", pendingRequestFd);
                    send(serverFd, cmd, strlen(cmd), 0);
                    
                    addLobbyMessage("*** Chat request declined ***");
                    
                    pendingRequestFd = -1;
                    memset(pendingRequestName, 0, sizeof(pendingRequestName));
                } else {
                    addLobbyMessage("*** No pending chat request ***");
                }
                memset(inputBuffer, 0, sizeof(inputBuffer));
                inputPos = 0;
                return;
            }
            
            char msg[MAX_INPUT + 32];
            snprintf(msg, sizeof(msg), "%s: %s", myUsername, inputBuffer);
            addLobbyMessage(msg);
            
            send(serverFd, msg, strlen(msg), 0);
            
            memset(inputBuffer, 0, sizeof(inputBuffer));
            inputPos = 0;
        }
    } else if (activePanel == 0) {
        // Request private chat with selected active user
        User *selected = getUserByStatusIndex(userlist, 1, activeSelected);
        if (selected && selected->fd != myFd) {
            // Send request to server
            char cmd[64];
            snprintf(cmd, sizeof(cmd), "/request %d", selected->fd);
            send(serverFd, cmd, strlen(cmd), 0);
            
            char msg[64];
            snprintf(msg, sizeof(msg), "*** Chat request sent to %s (waiting for response) ***", selected->username);
            addLobbyMessage(msg);
            
            // Store who we requested (for when they accept)
            strncpy(partnerName, selected->username, sizeof(partnerName) - 1);
            partnerFd = selected->fd;
        }
    } else if (activePanel == 1) {
        // Request private chat with selected online user
        User *selected = getUserByStatusIndex(userlist, 0, onlineSelected);
        if (selected && selected->fd != myFd) {
            char cmd[64];
            snprintf(cmd, sizeof(cmd), "/request %d", selected->fd);
            send(serverFd, cmd, strlen(cmd), 0);
            
            char msg[64];
            snprintf(msg, sizeof(msg), "*** Chat request sent to %s (waiting for response) ***", selected->username);
            addLobbyMessage(msg);
            
            strncpy(partnerName, selected->username, sizeof(partnerName) - 1);
            partnerFd = selected->fd;
        }
    }
}

void handlePrivateEnter() {
    if (inputPos > 0) {
        inputBuffer[inputPos] = '\0';
        
        // Check for /leave command
        if (strcmp(inputBuffer, "/leave") == 0) {
            switchToLobbyView();
            memset(inputBuffer, 0, sizeof(inputBuffer));
            inputPos = 0;
            return;
        }
        
        char msg[MAX_INPUT + 32];
        snprintf(msg, sizeof(msg), "You: %s", inputBuffer);
        addPrivateMessage(msg);
        
        send(serverFd, inputBuffer, strlen(inputBuffer), 0);
        
        memset(inputBuffer, 0, sizeof(inputBuffer));
        inputPos = 0;
    }
}

void handleKeyInput(int ch) {
    if (currentView == VIEW_LOBBY) {
        int *currentSelected = NULL;
        int currentCount = 0;
        
        if (activePanel == 0) {
            currentSelected = &activeSelected;
            currentCount = countUsersByStatus(userlist, 1);
        } else if (activePanel == 1) {
            currentSelected = &onlineSelected;
            currentCount = countUsersByStatus(userlist, 0);
        } else if (activePanel == 2) {
            currentSelected = &privateSelected;
            currentCount = countUsersByStatus(userlist, 3);
        }

        switch (ch) {
            case KEY_UP:
                if (currentSelected && *currentSelected > 0) {
                    (*currentSelected)--;
                }
                break;
            case KEY_DOWN:
                if (currentSelected && *currentSelected < currentCount - 1) {
                    (*currentSelected)++;
                }
                break;
            case '\t':
                activePanel = (activePanel + 1) % 4;
                break;
            case '\n':
            case KEY_ENTER:
                handleLobbyEnter();
                break;
            case KEY_BACKSPACE:
            case 127:
            case 8:
                if (activePanel == 3 && inputPos > 0) {
                    inputBuffer[--inputPos] = '\0';
                }
                break;
            default:
                if (activePanel == 3 && ch >= 32 && ch < 127 && inputPos < MAX_INPUT - 1) {
                    inputBuffer[inputPos++] = ch;
                    inputBuffer[inputPos] = '\0';
                }
                break;
        }
    } else {
        // Private view
        switch (ch) {
            case '\n':
            case KEY_ENTER:
                handlePrivateEnter();
                break;
            case KEY_BACKSPACE:
            case 127:
            case 8:
                if (inputPos > 0) {
                    inputBuffer[--inputPos] = '\0';
                }
                break;
            default:
                if (ch >= 32 && ch < 127 && inputPos < MAX_INPUT - 1) {
                    inputBuffer[inputPos++] = ch;
                    inputBuffer[inputPos] = '\0';
                }
                break;
        }
    }
}

// MAIN
int main(int argc, char *argv[]) {
    // Prompt for username
    printf("=================================\n");
    printf("       Welcome to Chat!\n");
    printf("=================================\n");
    printf("Enter your username: ");
    fflush(stdout);
    
    if (fgets(myUsername, sizeof(myUsername), stdin) == NULL) {
        fprintf(stderr, "Failed to read username\n");
        return 1;
    }
    
    // Remove newline from username
    char *nl = strchr(myUsername, '\n');
    if (nl) {    
        *nl = '\0';
    }
    
    // If empty, use default
    if (strlen(myUsername) == 0) {
        char *login = getlogin();
        if (login) {
            strncpy(myUsername, login, sizeof(myUsername) - 1);
        } else {
            snprintf(myUsername, sizeof(myUsername), "user%d", getpid());
        }
        printf("Using default username: %s\n", myUsername);
    }
    
    // Connect to server
    printf("Connecting to %s:%s...\n", SERVER_HOSTNAME, SERVER_PORT);
    serverFd = connectToServer(SERVER_HOSTNAME, SERVER_PORT);
    if (serverFd < 0) {
        fprintf(stderr, "Failed to connect to server\n");
        return 1;
    }
    printf("Connected!\n");

    // Send username to server
    send(serverFd, myUsername, sizeof(myUsername), 0);

    // Get initial userlist
    printf("Getting user list...\n");
    getUserlist(serverFd);
    printf("Ready! Starting UI...\n");

    // Initialize ncurses
    initscr();
    cbreak();
    noecho();
    keypad(stdscr, TRUE);
    curs_set(0);
    start_color();
    
    init_pair(1, COLOR_GREEN, COLOR_BLACK);    // Active panel title
    init_pair(2, COLOR_GREEN, COLOR_BLACK);   // Private chat users
    init_pair(3, COLOR_CYAN, COLOR_BLACK);  // Self highlight

    getmaxyx(stdscr, maxY, maxX);
    
    clear();
    refresh();
    
    initLobbyWindows();
    
    char welcome[64];
    snprintf(welcome, sizeof(welcome), "*** Welcome %s! ***", myUsername);
    addLobbyMessage(welcome);
    addLobbyMessage("*** TAB to switch panels, ENTER to select/send ***");
    
    renderLobby();

    struct pollfd fds[2];
    fds[0].fd = serverFd;
    fds[0].events = POLLIN;
    fds[1].fd = STDIN_FILENO;
    fds[1].events = POLLIN;

    nodelay(stdscr, TRUE);

    int running = 1;
    while (running) {
        poll(fds, 2, 50);

        // Check for server messages
        if (fds[0].revents & POLLIN) {
            char buffer[256] = {0};
            int nbytes = recv(serverFd, buffer, sizeof(buffer) - 1, 0);
            
            if (nbytes <= 0) {
                addLobbyMessage("*** Disconnected from server ***");
                if (currentView == VIEW_LOBBY) renderLobby();
                else renderPrivateChat();
                sleep(2);
                break;
            }

            buffer[nbytes] = '\0';
            
            while (buffer[0] == '{') {
                changeUser(buffer);
                char *end = strchr(buffer, '}');
                if (end) {
                    memmove(buffer, end + 1, strlen(end + 1) + 1);
                } else {
                    buffer[0] = '\0';
                    break;
                }
            }
            
            if (strlen(buffer) == 0) {
                if (currentView == VIEW_LOBBY) renderLobby();
                else renderPrivateChat();
                continue;
            }
            
            // Check for server notifications
            if (strncmp(buffer, "/chatrequest ", 13) == 0) {
                // Someone wants to chat with us
                int requesterFd;
                char requesterName[24];
                if (sscanf(buffer, "/chatrequest %d %23s", &requesterFd, requesterName) == 2) {
                    pendingRequestFd = requesterFd;
                    strncpy(pendingRequestName, requesterName, sizeof(pendingRequestName) - 1);
                    
                    char msg[128];
                    snprintf(msg, sizeof(msg), "*** %s wants to chat! Type /accept or /decline ***", requesterName);
                    addLobbyMessage(msg);
                }
                if (currentView == VIEW_LOBBY) renderLobby();
                continue;
            }
            
            if (strncmp(buffer, "/accepted ", 10) == 0) {
                // Our request was accepted!
                int accepterFd;
                char accepterName[24];
                if (sscanf(buffer, "/accepted %d %23s", &accepterFd, accepterName) == 2) {
                    char msg[64];
                    snprintf(msg, sizeof(msg), "*** %s accepted your chat request! ***", accepterName);
                    addLobbyMessage(msg);
                    
                    // Switch to private view
                    switchToPrivateView(accepterName, accepterFd);
                    renderPrivateChat();
                }
                continue;
            }
            
            if (strncmp(buffer, "/declined ", 10) == 0) {
                // Our request was declined
                char declinerName[24];
                if (sscanf(buffer, "/declined %23s", declinerName) == 1) {
                    char msg[64];
                    snprintf(msg, sizeof(msg), "*** %s declined your chat request ***", declinerName);
                    addLobbyMessage(msg);
                    
                    // Clear pending partner info
                    partnerFd = -1;
                    memset(partnerName, 0, sizeof(partnerName));
                }
                if (currentView == VIEW_LOBBY) renderLobby();
                continue;
            }
            
            if (strncmp(buffer, "/partleft ", 10) == 0) {
                char leaverName[24];
                if (sscanf(buffer, "/partleft %23s", leaverName) == 1) {
                    addPrivateMessage("*** Partner left the chat ***");
                    renderPrivateChat();
                    sleep(1);
                    switchToLobbyViewLocal();
                    renderLobby();
                }
                continue;
            }
            
            // Add regular message if not empty
            char *newline = strchr(buffer, '\n');
            if (newline) *newline = '\0';
            
            if (strlen(buffer) > 0) {
                if (currentView == VIEW_PRIVATE) {
                    // Prepend partner's name to incoming messages
                    char formattedMsg[MAX_INPUT + 32];
                    snprintf(formattedMsg, sizeof(formattedMsg), "%s: %s", partnerName, buffer);
                    addPrivateMessage(formattedMsg);
                } else {
                    addLobbyMessage(buffer);
                }
            }
            
            if (currentView == VIEW_LOBBY) renderLobby();
            else renderPrivateChat();
        }

        // Check for keyboard input
        int ch = getch();
        if (ch != ERR) {
            if (ch == 27) {  // ESC Key
                running = 0;
            } else {
                handleKeyInput(ch);
                if (currentView == VIEW_LOBBY) renderLobby();
                else renderPrivateChat();
            }
        }
    }

    // Cleanup
    if (currentView == VIEW_LOBBY) {
        destroyLobbyWindows();
    } else {
        destroyPrivateWindows();
    }
    endwin();
    close(serverFd);

    // Free messages
    for (int i = 0; i < lobbyMsgCount; i++) free(lobbyMessages[i]);
    for (int i = 0; i < privateMsgCount; i++) free(privateMessages[i]);

    // Free userlist
    User *temp;
    while (userlist) {
        temp = userlist;
        userlist = userlist->next;
        free(temp);
    }

    printf("Chat ended.\n");
    return 0;
}
