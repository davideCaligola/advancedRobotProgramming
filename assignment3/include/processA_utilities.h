#include <ncurses.h>
#include <string.h>
#include <unistd.h>
#include <math.h>
#include <time.h>
#include <stdlib.h>

// Typedef for circle struct
typedef struct
{
    int x, y;
} CIRCLE;

// Circle variable definition
CIRCLE circle;
// Window for print and exit button
WINDOW *print_btn, *exit_btn;
// Mouse event variable
MEVENT event;

int BTN_SIZE_Y = 3;
int BTN_SIZE_X = 7;

// Method to instantiate button window
WINDOW *make_button(int btn_size_y, int btn_size_x, int top, int left)
{
    return newwin(btn_size_y, btn_size_x, top, left);
}

// Draw button with colored background
void draw_btn(WINDOW *btn, char label, int color)
{
    wbkgd(btn, COLOR_PAIR(color));
    wmove(btn, floor(BTN_SIZE_Y / 2), floor(BTN_SIZE_X / 2));

    attron(A_BOLD);
    waddch(btn, label);
    attroff(A_BOLD);

    wrefresh(btn);
}

// Utility method to check if button has been pressed
int check_button_pressed(WINDOW *btn, MEVENT *event)
{
    if (event->y >= btn->_begy && event->y < btn->_begy + BTN_SIZE_Y)
    {
        if (event->x >= btn->_begx && event->x < btn->_begx + BTN_SIZE_X)
        {
            return TRUE;
        }
    }
    return FALSE;
}

// Method to draw lateral elements of the UI (button)
void draw_side_ui()
{
    mvvline(0, COLS - BTN_SIZE_X - 1, ACS_VLINE, LINES);
    // print button - create and add to window
    print_btn = make_button(BTN_SIZE_Y, BTN_SIZE_X, (LINES - BTN_SIZE_Y) / 2, (COLS - BTN_SIZE_X));
    draw_btn(print_btn, 'P', 2);
    // exit button - create and add to window
    exit_btn = make_button(BTN_SIZE_Y, BTN_SIZE_X, 0, (COLS - BTN_SIZE_X));
    draw_btn(exit_btn, 'X', 3);
    refresh();
}

// Set circle's initial position in the center of the window
void set_circle()
{
    circle.y = LINES / 2;
    circle.x = COLS / 2;
}

// Draw filled circle according to its equation
void draw_circle()
{
    attron(COLOR_PAIR(1));
    mvaddch(circle.y, circle.x, '@');
    mvaddch(circle.y - 1, circle.x, '@');
    mvaddch(circle.y + 1, circle.x, '@');
    mvaddch(circle.y, circle.x - 1, '@');
    mvaddch(circle.y, circle.x + 1, '@');
    attroff(COLOR_PAIR(1));
    refresh();
}

// Move circle window according to user's input
void move_circle(int cmd)
{
    // First, clear previous circle positions
    mvaddch(circle.y, circle.x, ' ');
    mvaddch(circle.y - 1, circle.x, ' ');
    mvaddch(circle.y + 1, circle.x, ' ');
    mvaddch(circle.y, circle.x - 1, ' ');
    mvaddch(circle.y, circle.x + 1, ' ');

    // Move circle by one character based on cmd
    switch (cmd)
    {
    case KEY_LEFT:
        if (circle.x - 1 > 0)
        {
            circle.x--;
        }
        break;
    case KEY_RIGHT:
        if (circle.x + 1 < COLS - BTN_SIZE_X - 2)
        {
            circle.x++;
        }
        break;
    case KEY_UP:
        if (circle.y - 1 > 0)
        {
            circle.y--;
        }
        break;
    case KEY_DOWN:
        if (circle.y + 2 < LINES)
        {
            circle.y++;
        }
        break;
    default:
        break;
    }
    refresh();
}

CIRCLE getCircleCenter()
{
    return circle;
}

int getAreaLines()
{
    return LINES - 1; // minus 1 because of the
}

int getAreaColumns()
{
    return COLS - BTN_SIZE_X - 2;
}

void init_console_ui()
{
    // Initialize curses mode
    initscr();
    start_color();

    // Disable line buffering...
    cbreak();

    // Disable char echoing and blinking cursos
    noecho();
    nodelay(stdscr, TRUE);
    curs_set(0);

    init_pair(1, COLOR_BLACK, COLOR_GREEN);
    init_pair(2, COLOR_WHITE, COLOR_BLUE);
    init_pair(3, COLOR_WHITE, COLOR_RED);

    // Initialize UI elements
    set_circle();

    // Draw UI elements
    draw_circle();
    draw_side_ui();

    // Activate input listening (keybord + mouse events ...)
    keypad(stdscr, TRUE);
    mousemask(ALL_MOUSE_EVENTS, NULL);

    refresh();
}

void reset_console_ui()
{
    // Free resources
    delwin(print_btn);
    delwin(exit_btn);

    // Clear screen
    erase();
    refresh();

    // Initialize UI elements
    set_circle();

    // Draw UI elements
    draw_circle();
    draw_side_ui();
}
