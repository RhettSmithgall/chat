#include <ncurses.h>
#include <string.h>
#include <stdlib.h>

#define MAX_USERS 100
#define MAX_MESSAGES 1000
#define MAX_INPUT 256

typedef struct {
    char *items[MAX_USERS];
    int count;
    int selected;
    int scroll_offset;
} UserList;

typedef struct {
    char *messages[MAX_MESSAGES];
    int count;
    int scroll_offset;
} ChatLog;

typedef struct {
    WINDOW *win;
    int height;
    int width;
    int y;
    int x;
} Panel;

// Global state
UserList active_users = {0};
UserList online_users = {0};
UserList private_chats = {0};
ChatLog chatlog = {0};
char input_buffer[MAX_INPUT] = {0};
int input_pos = 0;
int active_panel = 0; // 0=active, 1=online, 2=private, 3=input

Panel left_panels[3];
Panel chat_panel;
Panel input_panel;

void init_userlist(UserList *list) {
    list->count = 0;
    list->selected = 0;
    list->scroll_offset = 0;
}

void add_user(UserList *list, const char *name) {
    if (list->count < MAX_USERS) {
        list->items[list->count] = strdup(name);
        list->count++;
    }
}

void add_message(const char *msg) {
    if (chatlog.count < MAX_MESSAGES) {
        chatlog.messages[chatlog.count] = strdup(msg);
        chatlog.count++;
    }
}

void draw_box_with_title(WINDOW *win, const char *title, int is_active) {
    box(win, 0, 0);
    if (is_active) {
        wattron(win, A_BOLD | COLOR_PAIR(1));
    }
    mvwprintw(win, 0, 2, " %s ", title);
    if (is_active) {
        wattroff(win, A_BOLD | COLOR_PAIR(1));
    }
}

void render_userlist(Panel *panel, UserList *list, const char *title, int is_active) {
    werase(panel->win);
    draw_box_with_title(panel->win, title, is_active);
    
    int display_height = panel->height - 2;
    int start = list->scroll_offset;
    int end = start + display_height;
    if (end > list->count) end = list->count;
    
    for (int i = start; i < end; i++) {
        int y = i - start + 1;
        if (i == list->selected && is_active) {
            wattron(panel->win, A_REVERSE);
        }
        mvwprintw(panel->win, y, 2, "%s", list->items[i]);
        if (i == list->selected && is_active) {
            wattroff(panel->win, A_REVERSE);
        }
    }
    
    wrefresh(panel->win);
}

void render_chatlog() {
    werase(chat_panel.win);
    draw_box_with_title(chat_panel.win, "Chat", 0);
    
    int display_height = chat_panel.height - 2;
    int start = chatlog.count > display_height ? chatlog.count - display_height : 0;
    
    for (int i = start; i < chatlog.count; i++) {
        int y = i - start + 1;
        mvwprintw(chat_panel.win, y, 2, "%s", chatlog.messages[i]);
    }
    
    wrefresh(chat_panel.win);
}

void render_input() {
    werase(input_panel.win);
    draw_box_with_title(input_panel.win, "Message", active_panel == 3);
    mvwprintw(input_panel.win, 1, 2, "%s", input_buffer);
    wrefresh(input_panel.win);
}

void render_all() {
    render_userlist(&left_panels[0], &active_users, "Active Users", active_panel == 0);
    render_userlist(&left_panels[1], &online_users, "Online Users", active_panel == 1);
    render_userlist(&left_panels[2], &private_chats, "Private Chats", active_panel == 2);
    render_chatlog();
    render_input();
}

void move_selection(UserList *list, int delta) {
    list->selected += delta;
    if (list->selected < 0) list->selected = 0;
    if (list->selected >= list->count) list->selected = list->count - 1;
    
    // Adjust scroll
    int display_height = left_panels[0].height - 2;
    if (list->selected < list->scroll_offset) {
        list->scroll_offset = list->selected;
    } else if (list->selected >= list->scroll_offset + display_height) {
        list->scroll_offset = list->selected - display_height + 1;
    }
}

void handle_enter() {
    if (active_panel == 3) {
        // Send message
        if (strlen(input_buffer) > 0) {
            char msg[MAX_INPUT + 10];
            snprintf(msg, sizeof(msg), "You: %s", input_buffer);
            add_message(msg);
            input_buffer[0] = '\0';
            input_pos = 0;
        }
    } else if (active_panel == 0 && active_users.count > 0) {
        // Send chat request
        char msg[MAX_INPUT];
        snprintf(msg, sizeof(msg), "Chat request sent to %s", 
                 active_users.items[active_users.selected]);
        add_message(msg);
    }
}

int main() {
    initscr();
    cbreak();
    noecho();
    keypad(stdscr, TRUE);
    curs_set(1);
    
    start_color();
    init_pair(1, COLOR_CYAN, COLOR_BLACK);
    
    int max_y, max_x;
    getmaxyx(stdscr, max_y, max_x);
    
    int left_width = max_x / 3;
    int right_width = max_x - left_width;
    int left_panel_height = max_y / 3;
    
    // Create left panels
    for (int i = 0; i < 3; i++) {
        left_panels[i].height = left_panel_height;
        left_panels[i].width = left_width;
        left_panels[i].y = i * left_panel_height;
        left_panels[i].x = 0;
        left_panels[i].win = newwin(left_panel_height, left_width, 
                                    left_panels[i].y, left_panels[i].x);
    }
    
    // Create chat panel
    chat_panel.height = max_y - 4;
    chat_panel.width = right_width;
    chat_panel.y = 0;
    chat_panel.x = left_width;
    chat_panel.win = newwin(chat_panel.height, chat_panel.width, 
                           chat_panel.y, chat_panel.x);
    
    // Create input panel
    input_panel.height = 4;
    input_panel.width = right_width;
    input_panel.y = max_y - 4;
    input_panel.x = left_width;
    input_panel.win = newwin(input_panel.height, input_panel.width, 
                            input_panel.y, input_panel.x);
    
    // Initialize data
    init_userlist(&active_users);
    init_userlist(&online_users);
    init_userlist(&private_chats);
    
    // Sample data
    add_user(&active_users, "Alice");
    add_user(&active_users, "Bob");
    add_user(&online_users, "Charlie");
    add_user(&online_users, "Diana");
    add_user(&online_users, "Eve");
    add_user(&private_chats, "Frank");
    add_message("Welcome to the chat!");
    add_message("Press TAB to switch panels, arrows to navigate");
    
    render_all();
    
    int ch;
    while ((ch = getch()) != 27) { // ESC to quit
        UserList *current_list = NULL;
        if (active_panel == 0) current_list = &active_users;
        else if (active_panel == 1) current_list = &online_users;
        else if (active_panel == 2) current_list = &private_chats;
        
        switch(ch) {
            case KEY_UP:
                if (current_list) move_selection(current_list, -1);
                break;
            case KEY_DOWN:
                if (current_list) move_selection(current_list, 1);
                break;
            case '\t': // TAB
                active_panel = (active_panel + 1) % 4;
                break;
            case '\n': // ENTER
                handle_enter();
                break;
            case KEY_BACKSPACE:
            case 127:
                if (active_panel == 3 && input_pos > 0) {
                    input_buffer[--input_pos] = '\0';
                }
                break;
            default:
                if (active_panel == 3 && ch >= 32 && ch < 127 && input_pos < MAX_INPUT - 1) {
                    input_buffer[input_pos++] = ch;
                    input_buffer[input_pos] = '\0';
                }
                break;
        }
        
        render_all();
    }
    
    // Cleanup
    for (int i = 0; i < 3; i++) {
        delwin(left_panels[i].win);
    }
    delwin(chat_panel.win);
    delwin(input_panel.win);
    endwin();
    
    return 0;
}
