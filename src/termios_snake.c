

#include <unistd.h>
#include <errno.h>
#include <termios.h>
#include <sys/ioctl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h> 

#include <time.h>

#define CONST_WRITE(a) (write(STDOUT_FILENO, (a), sizeof((a))))
#define VAR_WRITE(a) (write(STDOUT_FILENO, (a), strlen((a))))

/*

Esc[Value;m 	Set Graphics Mode:
 
Text attributes
0	All attributes off
1	Bold on
4	Underscore (on monochrome display adapter only)
5	Blink on
7	Reverse video on
8	Concealed on
 
Foreground colors
30	Black
31	Red
32	Green
33	Yellow
34	Blue
35	Magenta
36	Cyan
37	White
 
Background colors
40	Black
41	Red
42	Green
43	Yellow
44	Blue
45	Magenta
46	Cyan
47	White
 

*/


/*** screen manip ***/

int screenGetSize(int* width, int* height) {
    
    struct winsize ws;
    
    if (ioctl(STDOUT_FILENO, TIOCGWINSZ, &ws) == -1 || ws.ws_col == 0) {
        return -1;
    }

    *width = ws.ws_col;
    *height = ws.ws_row;
    
    return 0;
};

int screenRollbackPointer() {

    CONST_WRITE("\x1b[H");
    
    return 0;
}

int screenRollbackColor() {

    CONST_WRITE("\x1b[0;m");
    
    return 0;
}

int screenRefreshFrame() {
    
    screenRollbackPointer();
    screenRollbackColor();

    CONST_WRITE("\x1b[2J");
    
    return 0;   
}

/* TODO: handle out of bounds */
int screenPositionCursor(int x, int y) {

    char command[255];

    command[0] = '\0';

    sprintf(command, "\x1b[%d;%dH", y, x);
    
    VAR_WRITE(command);    

    return 0;
}

int screenSetColor(int color, int color2, int is_bold) {

    char command[255];

    command[0] = '\0';

    //is_bold = !is_bold;

    if (color2 > 0)
        sprintf(command, "\x1b[%d;%d;%dm", is_bold, color, color2);
    else
        sprintf(command, "\x1b[%d;%dm", is_bold, color);
    
    VAR_WRITE(command);    

    return 0;
}

/* TODO: optimize for one write-call */
int screenFillFrame(char filler, int width, int height) {
    
    screenRollbackPointer();
    screenRollbackColor();

    //height-=2;

    while (height != 0) {
        
        int t_width = width;

        while (t_width != 0) {
            
            write(STDOUT_FILENO, &filler, 1);

            t_width--;
        }

        if (height != 1)
             CONST_WRITE("\r\n");

        height--;
    }
    
    screenRollbackPointer();

    return 0;
}

/* TODO: optimize for one write-call */
int screenRefreshNoFrame(char filler) {
    
    screenRollbackPointer();
    screenRollbackColor();

    int width = 0;
    int height = 0;

    screenGetSize(&width, &height);

    //height-=2;

    while (height != 0) {
        
        int t_width = width;

        while (t_width != 0) {
            
            write(STDOUT_FILENO, &filler, 1);
            t_width--;
        }

        if (height != 1)
             CONST_WRITE("\r\n");

        height--;
    }
    
    screenRollbackPointer();

    return 0;
}

int __screenReadKey(int *key, int *esc) {
    
     /* TODO: add multiple screenRead, so that only the last one returns the value */

    int r;
    char k;

    r = read(STDOUT_FILENO, &k, 1);
    
    if (r == 1) {
        r = k;
    }
    else {
        r = -1;
    }

    *key = r;
    *esc = 0;

    if (r == '\033') {

        char k2[2];
        read(STDOUT_FILENO, &k2, 2);
        *key = k2[1];
        *esc = 1;
    }
    
    return *key;

}


int screenReadKey(int *key, int *esc) {
   
    __screenReadKey(key, esc);

    int lkey, lesc;

    while (__screenReadKey(&lkey, &lesc) > -1) {
    
        *key = lkey;
        *esc = lesc;

    }
    
    return *key;
}

/*** termios manip ***/

struct termios old_termios;

void reset_termios() {
    
    tcsetattr(STDIN_FILENO, TCSANOW, &old_termios);
    screenRefreshFrame();

    exit(0);
}

int setup_termios() {

    tcgetattr(STDIN_FILENO, &old_termios);
    
    atexit(reset_termios);
    signal(SIGINT, reset_termios);

    struct termios new_termios = old_termios;

    /* TODO: return signals back */
    cfmakeraw(&new_termios);
    new_termios.c_lflag |= ISIG;

    new_termios.c_cc[VMIN] = 0;
    new_termios.c_cc[VTIME] = 0;

    tcsetattr(STDIN_FILENO, TCSANOW, &new_termios);
}

/*** snake game ***/

/*
    In a nutshell, snake consists of a field M * N
    Each cell is either 0 or a value.

    If 0, there is no snake, if a value, then there is.

    Like:

    0 0 0 0 0 0
    0 7 6 5 0 0
    0 0 3 4 0 0
    0 0 2 0 0 0

    So that, the tail is always aware of where to move and
    we don't have to store the snake neither as a linked list
    nor as something else redundant.

    Each move, a value under the snake's head increases it's number.
    
*/

struct SnakeGame {

    int width;
    int height;
    
    // if 0 - no snake, if > 0, it is snake's segment
    unsigned int* field_flags;
    
    int head_x;
    int head_y;

    int tail_x;
    int tail_y;

    int snake_size;

    int food_x;
    int food_y;

};

void snakeSetFlags(struct SnakeGame *game, int x, int y, int value) {

    if (x < 0 || y < 0 || x >= game->width || y >= game->height)
        return;
    
    game->field_flags[y * game->width + x] = value;   
        
}

int snakeGetFlags(struct SnakeGame *game, int x, int y) {

    if (x < 0 || y < 0 || x >= game->width || y >= game->height)
        return 0;

    return game->field_flags[y * game->width + x];   
    
}

void snakeAllocateFood(struct SnakeGame *game) {

    game->food_x = rand() % game->width;
    game->food_y = rand() % game->height;

    // food allocated on the snake itself, then we move it to the first empty place
    if (snakeGetFlags(game, game->food_x, game->food_y) > 0) {

        int field_flags_id = game->food_y * game->width + game->food_x;        
        int field_flags_size = game->width * game->height;
        int attempts = field_flags_size;

        while (attempts--) {

            field_flags_id++;

            int actual_id = field_flags_id % field_flags_size;

            if (game->field_flags[actual_id] == 0) {
                game->food_x = (actual_id) % game->width;                
                game->food_y = (actual_id) / game->width;
            }
        }
        
    }
    
}

void snakeInit(struct SnakeGame *game) {
    
    game->width = 30;
    game->height = 20;

    game->field_flags = calloc(game->width * game->height, sizeof(unsigned int));
    
    game->head_x = 15;
    game->head_y = 10;

    game->tail_x = game->head_x;
    game->tail_y = game->head_y;

    game->snake_size = 2;

    snakeSetFlags(game, game->head_x, game->head_y, 1);
    
    snakeAllocateFood(game);

};

void snakeRender(struct SnakeGame *game) {
    
    for (unsigned int vy = 0; vy < game->height; ++vy) {
        for (unsigned int vx = 0; vx < game->width; ++vx) {
            unsigned int at_pos = snakeGetFlags(game, vx, vy);

            screenPositionCursor(vx * 2 + 1, vy + 1);

            if (at_pos == 0) {
                if (game->food_x == vx && game->food_y == vy) {
                    screenSetColor(47, 30, 1);
                    CONST_WRITE("^^");
                }
                else {
                    screenRollbackColor();
                    CONST_WRITE("  ");
                }
            }
            else {
                if (game->head_x == vx && game->head_y == vy) {
                    screenSetColor(33, 41, 1);

                    if (abs(game->food_x - game->head_x) + abs(game->food_y - game->head_y) == 1)
                        CONST_WRITE(":D");
                    else
                        CONST_WRITE(":)");
                }
                else {
                    screenSetColor(42, 37, 1);
                    CONST_WRITE("  ");
                }
            }
        }
    }
    
    screenRollbackPointer();

}

int snakeProcess(struct SnakeGame *game, int dx, int dy) {

    unsigned int at_head = snakeGetFlags(game, game->head_x, game->head_y);
    unsigned int at_tail = snakeGetFlags(game, game->tail_x, game->tail_y);
    
    
    int dir_x = dx;
    int dir_y = dy;

    int next_head_x = (int)(game->head_x) + dir_x;
    int next_head_y = (int)(game->head_y) + dir_y;

    if (game->snake_size + 2 == game->width * game->height) {
        return 2;
    }

    if (next_head_x < 0 || next_head_x >= game->width || next_head_y < 0 || next_head_y >= game->height) {
        return 1;
    }

    if (snakeGetFlags(game, next_head_x, next_head_y)) {
        return 1;
    }

    // move the head
    game->head_x = next_head_x;
    game->head_y = next_head_y;
    snakeSetFlags(game, game->head_x, game->head_y, at_head + 1);

    //now move the tail if it's time
    if (game->snake_size + at_tail < at_head) {

        int dir_tail_x = 0;
        int dir_tail_y = 0;
        
        //find, on which cell the value is different only as 1
        if (snakeGetFlags(game, game->tail_x + 1, game->tail_y) == 1 + at_tail) {
            dir_tail_x = 1;
        }
        else if (snakeGetFlags(game, game->tail_x - 1, game->tail_y) == 1 + at_tail) {
            dir_tail_x = -1;
        }
        else if (snakeGetFlags(game, game->tail_x, game->tail_y + 1) == 1 + at_tail) {
            dir_tail_y = 1;
        }
        else if (snakeGetFlags(game, game->tail_x, game->tail_y - 1) == 1 + at_tail) {
            dir_tail_y = -1;
        }

        snakeSetFlags(game, game->tail_x, game->tail_y, 0);

        game->tail_x += dir_tail_x;
        game->tail_y += dir_tail_y;
    
    }

    //eat food
    if (game->head_x == game->food_x && game->head_y == game->food_y) { 

        snakeAllocateFood(game);

        game->snake_size++;

    }

    return 0;

}

void renderFrame(struct SnakeGame *game) {

    screenRefreshFrame();
    screenRefreshNoFrame('~');
    screenFillFrame(' ', game->width * 2, game->height);
    
    screenRollbackPointer();

}


/*** logic manip ***/


/*
    
    TODO:

    1) offset rendered map and add game instructions
    2) add quit
    3) drop string constants and magic numbers out
    4) redraw snake in a buffer and commit each frame rendering with a single WRITE only
    5) add :( to snake's face when lost
    6) Handle errors and print perror at exit
    7) rehandle keyboard events, so that at least 1 key is buffered back

*/

#define AUTOMATON_GAME 0
#define AUTOMATON_PAUSE 1
#define AUTOMATON_LOSE 2
#define AUTOMATON_WIN 3


int main() {

    srand(time(NULL));

    setup_termios();
    
    struct SnakeGame game;

    snakeInit(&game);

    renderFrame(&game);
    
    int dx, dy;

    dx = 1;
    dy = 0;

    int screenWidth, screenHeight;

    screenGetSize(&screenWidth, &screenHeight);

    int automaton_state = AUTOMATON_GAME;

    while (1) {

        int localScreenWidth, localScreenHeight;

        screenGetSize(&localScreenWidth, &localScreenHeight);

        if (localScreenWidth != screenWidth || localScreenHeight != screenHeight) {
            screenWidth = localScreenWidth;
            screenHeight = localScreenHeight;
            renderFrame(&game);
        }

        int key, esc;

        screenReadKey(&key, &esc);

        if (esc && key == 'A' && dy != 1) {
            dx = 0;
            dy = -1;
        }
        
        
        if (esc && key == 'B' && dy != -1) {
            dx = 0;
            dy = 1;            
        }

        
        if (esc && key == 'C' && dx != -1) {
            dx = 1;
            dy = 0;            
        }

        
        if (esc && key == 'D' && dx != 1) {
            dx = -1;
            dy = 0;
        }

        if (!esc && key == 'p') {
            if (automaton_state == AUTOMATON_GAME) {
                automaton_state = AUTOMATON_PAUSE;
            }
            else if (automaton_state == AUTOMATON_PAUSE) {
                automaton_state = AUTOMATON_GAME;
            }
        }

        if (automaton_state == AUTOMATON_PAUSE) {
            
            screenRollbackColor();
                
            screenPositionCursor(23, 9);

            CONST_WRITE("=================");

            screenPositionCursor(23, 10);

            CONST_WRITE("=     PAUSE     =");

            screenPositionCursor(23, 11);

            CONST_WRITE("=================");

            screenRollbackPointer();              
                        
        }

        if (automaton_state == AUTOMATON_LOSE) {
            
            screenRollbackColor();
                
            screenPositionCursor(23, 9);
            
            CONST_WRITE("=================");
            
            screenPositionCursor(23, 10);
            
            CONST_WRITE("= GAME OVER! :( =");
            
            screenPositionCursor(23, 11);
            
            CONST_WRITE("=================");
            
            screenRollbackPointer();
            
            sleep(3);            

            return 0;
            
        }

        if (automaton_state == AUTOMATON_WIN) {
            
            screenRollbackColor();
                
            screenPositionCursor(23, 9);
            
            CONST_WRITE("=================");
            
            screenPositionCursor(23, 10);
            
            CONST_WRITE("=  YOU WON!  :D =");
            
            screenPositionCursor(23, 11);
            
            CONST_WRITE("=================");
            
            screenRollbackPointer();
            
            sleep(3);              

            return 0;
            
        }

        if (automaton_state == AUTOMATON_GAME) {

            int game_result = snakeProcess(&game, dx, dy);

            snakeRender(&game);
            
            if (game_result == 1) {
                automaton_state = AUTOMATON_LOSE;
            }
            
            if (game_result == 2) {
                automaton_state = AUTOMATON_WIN;                
            }

        }
        
        usleep(100000);
    }
    
    return 0;
}





















