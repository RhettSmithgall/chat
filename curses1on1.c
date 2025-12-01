#include <ncurses.h>
#include <string.h>
#include <stdlib.h>

#define INPUT_HEIGHT 3

WINDOW *partner_win, *user_win, *input_win;
int max_y, max_x;

void init_windows() {
    getmaxyx(stdscr, max_y, max_x);
    
    int chat_height = (max_y - INPUT_HEIGHT) / 2;
    
    // Partner's window (top half)
    partner_win = newwin(chat_height, max_x, 0, 0);
    box(partner_win, 0, 0);
    mvwprintw(partner_win, 0, 2, " Partner ");
    wrefresh(partner_win);
    
    // User's window (bottom half of chat area)
    user_win = newwin(chat_height, max_x, chat_height, 0);
    box(user_win, 0, 0);
    mvwprintw(user_win, 0, 2, " You ");
    wrefresh(user_win);
    
    // Input window (bottom of screen)
    input_win = newwin(INPUT_HEIGHT, max_x, max_y - INPUT_HEIGHT, 0);
    box(input_win, 0, 0);
    mvwprintw(input_win, 0, 2, " Type message (Ctrl+D to quit) ");
    wmove(input_win, 1, 1);
    wrefresh(input_win);
}

void add_message(WINDOW *win, const char *prefix, const char *msg) {
    int win_y, win_x;
    getmaxyx(win, win_y, win_x);
    
    // Scroll window content up
    scrollok(win, TRUE);
    wmove(win, win_y - 2, 1);
    waddstr(win, prefix);
    waddstr(win, msg);
    waddch(win, '\n');
    
    box(win, 0, 0);
    if (win == partner_win)
        mvwprintw(win, 0, 2, " Partner ");
    else
        mvwprintw(win, 0, 2, " You ");
    
    wrefresh(win);
}

void cleanup() {
    delwin(partner_win);
    delwin(user_win);
    delwin(input_win);
    endwin();
}

int main() {
    initscr();
    cbreak();
    noecho();
    keypad(stdscr, TRUE);
    
    init_windows();
    
    char input[256];
    int pos = 0;
    int ch;
    
    // Simulate some initial messages
    add_message(partner_win, "", "Partner: Hello! How are you?");
    add_message(user_win, "", "You: Hi there! I'm good, thanks!");
    
    wmove(input_win, 1, 1);
    wrefresh(input_win);
    
    while (1) {
        ch = wgetch(input_win);
        
        if (ch == 4) { // Ctrl+D
            break;
        } else if (ch == '\n' || ch == KEY_ENTER) {
            if (pos > 0) {
                input[pos] = '\0';
                
                // Add message to user's window
                add_message(user_win, "You: ", input);
                
                // Clear input area
                wclear(input_win);
                box(input_win, 0, 0);
                mvwprintw(input_win, 0, 2, " Type message (Ctrl+D to quit) ");
                wmove(input_win, 1, 1);
                wrefresh(input_win);
                
                pos = 0;
                memset(input, 0, sizeof(input));
            }
        } else if (ch == KEY_BACKSPACE || ch == 127 || ch == 8) {
            if (pos > 0) {
                pos--;
                int y, x;
                getyx(input_win, y, x);
                mvwaddch(input_win, y, x - 1, ' ');
                wmove(input_win, y, x - 1);
                wrefresh(input_win);
            }
        } else if (ch >= 32 && ch < 127 && pos < 254) {
            input[pos++] = ch;
            waddch(input_win, ch);
            wrefresh(input_win);
        }
    }
    
    cleanup();
    printf("Chat ended.\n");
    
    return 0;
}
