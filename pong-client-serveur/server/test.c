#include <stdio.h>
#include <unistd.h>
#include <termios.h>
#include <fcntl.h>
#include <string.h>
#include <stdlib.h>
#include "game.h"

/* ================= Terminal Raw Mode ================= */

static struct termios orig_termios;

void disable_raw_mode() {
    tcsetattr(STDIN_FILENO, TCSAFLUSH, &orig_termios);
}

void enable_raw_mode() {
    tcgetattr(STDIN_FILENO, &orig_termios);
    atexit(disable_raw_mode);

    struct termios raw = orig_termios;
    raw.c_lflag &= ~(ECHO | ICANON); /* no echo, no canonical mode */
    raw.c_cc[VMIN] = 0;
    raw.c_cc[VTIME] = 0;

    tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw);
}

/* ================= Keyboard ================= */

int kbhit() {
    int oldf = fcntl(STDIN_FILENO, F_GETFL);
    fcntl(STDIN_FILENO, F_SETFL, oldf | O_NONBLOCK);

    int ch = getchar();

    fcntl(STDIN_FILENO, F_SETFL, oldf);

    if (ch != EOF) {
        ungetc(ch, stdin);
        return 1;
    }
    return 0;
}

int getch_noblock() {
    if (!kbhit()) return -1;
    return getchar();
}

/* ================= Rendering ================= */

#define TERM_W 80
#define TERM_H 24

void draw_game(const GameState *g) {
    char screen[TERM_H][TERM_W + 1];

    for (int y = 0; y < TERM_H; y++) {
        for (int x = 0; x < TERM_W; x++) screen[y][x] = ' ';
        screen[y][TERM_W] = '\0';
    }

    /* borders */
    for (int x = 0; x < TERM_W; x++) {
        screen[0][x] = '-';
        screen[TERM_H - 1][x] = '-';
    }
    for (int y = 0; y < TERM_H; y++) {
        screen[y][0] = '|';
        screen[y][TERM_W - 1] = '|';
    }

    /* dashed center line */
    for (int y = 1; y < TERM_H - 1; y += 2) {
        screen[y][TERM_W / 2] = '|';
    }

    /* score */
    char score[32];
    snprintf(score, sizeof(score), " %d : %d ", g->score_left, g->score_right);
    int start = (TERM_W - (int)strlen(score)) / 2;
    for (int i = 0; score[i]; i++) {
        screen[0][start + i] = score[i];
    }

    /* logical -> screen scale */
    float sx = (TERM_W - 2) / g->field_w;
    float sy = (TERM_H - 2) / g->field_h;

    /* paddles */
    int paddle_h = (int)(g->paddle_h * sy);
    int px_left = 2;
    int px_right = TERM_W - 3;

    int py_left = (int)(g->paddle_left_y * sy);
    int py_right = (int)(g->paddle_right_y * sy);

    for (int i = -paddle_h / 2; i <= paddle_h / 2; i++) {
        int y1 = py_left + i;
        int y2 = py_right + i;
        if (y1 > 0 && y1 < TERM_H - 1) screen[y1][px_left] = '#';
        if (y2 > 0 && y2 < TERM_H - 1) screen[y2][px_right] = '#';
    }

    /* ball */
    int bx = (int)(g->ball_x * sx);
    int by = (int)(g->ball_y * sy);

    if (bx > 0 && bx < TERM_W - 1 && by > 0 && by < TERM_H - 1) {
        screen[by][bx] = 'O';
    }

    /* clear + print */
    printf("\033[H\033[J");
    for (int y = 0; y < TERM_H; y++) {
        printf("%s\n", screen[y]);
    }
    fflush(stdout);
}

/* ================= Main ================= */

int main() {
    GameState game;
    game_init(&game);

    enable_raw_mode();

    printf("PONG ASCII TEST\n");
    printf("Controls: W/S = Left | Up/Down = Right | Q = Quit\n");
    usleep(1000000);

    while (1) {
        PlayerInput left = INPUT_NONE;
        PlayerInput right = INPUT_NONE;

        int ch = getch_noblock();
        if (ch != -1) {
            if (ch == 'q' || ch == 'Q') break;

            if (ch == 'w' || ch == 'W') left = INPUT_UP;
            if (ch == 's' || ch == 'S') left = INPUT_DOWN;

            if (ch == 27) { /* escape sequence for arrows */
                if (kbhit()) getchar(); /* skip '[' */
                int dir = getchar();
                if (dir == 'A') right = INPUT_UP;    /* up */
                if (dir == 'B') right = INPUT_DOWN;  /* down */
            }
        }

        game_step(&game, left, right);
        draw_game(&game);

        usleep((useconds_t)(game.dt * 1000000.0f));
    }

    return 0;
}
