#include <stdio.h>
#include <unistd.h>
#include <termios.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include "game.h"

/* ================= Terminal Raw Mode ================= */

static struct termios orig_termios;

static void disable_raw_mode(void) {
    tcsetattr(STDIN_FILENO, TCSAFLUSH, &orig_termios);
}

static void enable_raw_mode(void) {
    tcgetattr(STDIN_FILENO, &orig_termios);
    atexit(disable_raw_mode);

    struct termios raw = orig_termios;
    raw.c_lflag &= ~(ECHO | ICANON); /* no echo, no canonical mode */
    raw.c_cc[VMIN] = 0;
    raw.c_cc[VTIME] = 0;

    tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw);
}

/* ================= Time helpers ================= */

static double now_s(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (double)ts.tv_sec + (double)ts.tv_nsec / 1e9;
}

/* If no repeat received during this time, we consider the key released */
#define RELEASE_TIMEOUT_S 0.12  /* 120ms: adjust if needed */

/* ================= Keyboard (non-blocking) ================= */

static int read_key_nonblock(void) {
    unsigned char c;
    int n = read(STDIN_FILENO, &c, 1);
    if (n == 1) return (int)c;
    return -1;
}

/* Returns:
 *  - normal ASCII char (e.g. 'w', 's', 'q')
 *  - special codes: 1001 = UP, 1002 = DOWN
 *  - -1 if no key available
 */
#define KEY_UP   1001
#define KEY_DOWN 1002

static int read_key_decoded(void) {
    int c = read_key_nonblock();
    if (c == -1) return -1;

    if (c == 27) { /* ESC */
        int c1 = read_key_nonblock(); /* usually '[' */
        int c2 = read_key_nonblock(); /* 'A' or 'B' */
        if (c1 == '[') {
            if (c2 == 'A') return KEY_UP;
            if (c2 == 'B') return KEY_DOWN;
        }
        return -1; /* unknown/partial escape sequence */
    }

    return c; /* normal key */
}

/* ================= Rendering ================= */

#define TERM_W 80
#define TERM_H 24

static void draw_game(const GameState *g) {
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

    /* center line */
    for (int y = 1; y < TERM_H - 1; y += 2)
        screen[y][TERM_W / 2] = '|';

    /* score */
    char score[32];
    snprintf(score, sizeof(score), " %d : %d ", g->score_left, g->score_right);
    int start = (TERM_W - (int)strlen(score)) / 2;
    for (int i = 0; score[i]; i++)
        screen[0][start + i] = score[i];

    /* logical -> screen scale */
    float sx = (TERM_W - 2) / g->field_w;
    float sy = (TERM_H - 2) / g->field_h;

    /* paddles */
    int ph = (int)(g->paddle_h * sy);
    int pxL = 2;
    int pxR = TERM_W - 3;

    int pyL = (int)(g->paddle_left_y * sy);
    int pyR = (int)(g->paddle_right_y * sy);

    for (int i = -ph / 2; i <= ph / 2; i++) {
        int yL = pyL + i;
        int yR = pyR + i;
        if (yL > 0 && yL < TERM_H - 1) screen[yL][pxL] = '#';
        if (yR > 0 && yR < TERM_H - 1) screen[yR][pxR] = '#';
    }

    /* ball */
    int bx = (int)(g->ball_x * sx);
    int by = (int)(g->ball_y * sy);
    if (bx > 0 && bx < TERM_W - 1 && by > 0 && by < TERM_H - 1)
        screen[by][bx] = 'O';

    printf("\033[H\033[J");
    for (int y = 0; y < TERM_H; y++)
        printf("%s\n", screen[y]);
    fflush(stdout);
}

/* ================= Main ================= */

int main(void) {
    GameState game;
    game_init(&game);

    enable_raw_mode();

    printf("PONG ASCII TEST\n");
    printf("Controls:\n");
    printf("  Joueur gauche : W (haut) / S (bas)\n");
    printf("  Joueur droite : Fleche Haut / Fleche Bas\n");
    printf("  Q             : Quit\n");
    usleep(700000);

    int left_dir = 0;   /* -1 up, +1 down, 0 none */
    int right_dir = 0;

    /* last time we received a repeat for each player */
    double last_left_input  = 0.0;
    double last_right_input = 0.0;

    while (1) {
        double t = now_s();

        /* If no repeat for a while -> consider released -> stop */
        if (left_dir != 0 && (t - last_left_input) > RELEASE_TIMEOUT_S) {
            left_dir = 0;
        }
        if (right_dir != 0 && (t - last_right_input) > RELEASE_TIMEOUT_S) {
            right_dir = 0;
        }

        int key = read_key_decoded();
        if (key != -1) {
            if (key == 'q' || key == 'Q') break;

            /* Left paddle: W/S */
            if (key == 'w' || key == 'W') {
                left_dir = -1;
                last_left_input = t;
            } else if (key == 's' || key == 'S') {
                left_dir = +1;
                last_left_input = t;
            }

            /* Right paddle: arrow up/down */
            if (key == KEY_UP) {
                right_dir = -1;
                last_right_input = t;
            } else if (key == KEY_DOWN) {
                right_dir = +1;
                last_right_input = t;
            }
        }

        PlayerInput left =
            (left_dir < 0) ? INPUT_UP :
            (left_dir > 0) ? INPUT_DOWN :
            INPUT_NONE;

        PlayerInput right =
            (right_dir < 0) ? INPUT_UP :
            (right_dir > 0) ? INPUT_DOWN :
            INPUT_NONE;

        game_step(&game, left, right);
        draw_game(&game);

        usleep((useconds_t)(game.dt * 1000000.0f));
    }

    return 0;
}
