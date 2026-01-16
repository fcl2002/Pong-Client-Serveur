// client_tcp.c - Pong TCP client with client-side prediction + improved reconciliation
// Controls: W/S for your paddle (both players). Q quits.
// Build: gcc client_tcp.c -o client_tcp
// Run  : ./client_tcp 127.0.0.1 5555
#define _POSIX_C_SOURCE 200809L
#include <arpa/inet.h>
#include <netinet/in.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <termios.h>
#include <time.h>
#include <unistd.h>

/* ================= Protocol ================= */

enum { MSG_HELLO = 1, MSG_INPUT = 2, MSG_STATE = 3 };

typedef struct __attribute__((packed)) {
    uint8_t type;
    uint8_t player_id;  // 1 or 2
    uint8_t dir;        // 0 none, 1 up, 2 down
    uint8_t _pad;
} MsgInput;

typedef struct __attribute__((packed)) {
    uint8_t type;
    uint8_t player_id;  // 1 or 2
    uint16_t _pad;
} MsgHello;

typedef struct __attribute__((packed)) {
    uint16_t tick;
    int16_t  ball_x;
    int16_t  ball_y;
    int16_t  paddle_left_y;
    int16_t  paddle_right_y;
    uint16_t score_left;
    uint16_t score_right;
    uint16_t field_w;
    uint16_t field_h;
    uint16_t paddle_h;
    uint16_t ball_size;
} NetState;

typedef struct __attribute__((packed)) {
    uint8_t  type;
    uint8_t  _pad;
    uint16_t size;
    NetState st;
} MsgState;

/* ================= TCP helpers ================= */

static int send_all(int fd, const void *buf, size_t len) {
    const uint8_t *p = (const uint8_t *)buf;
    size_t sent = 0;
    while (sent < len) {
        ssize_t n = send(fd, p + sent, len - sent, 0);
        if (n <= 0) return -1;
        sent += (size_t)n;
    }
    return 0;
}

static int recv_all(int fd, void *buf, size_t len) {
    uint8_t *p = (uint8_t *)buf;
    size_t recvd = 0;
    while (recvd < len) {
        ssize_t n = recv(fd, p + recvd, len - recvd, 0);
        if (n <= 0) return (int)n; // 0 disconnect, -1 error
        recvd += (size_t)n;
    }
    return 1;
}

/* ================= Terminal raw mode ================= */

static struct termios orig_termios;

static void disable_raw_mode(void) {
    tcsetattr(STDIN_FILENO, TCSAFLUSH, &orig_termios);
}

static void enable_raw_mode(void) {
    tcgetattr(STDIN_FILENO, &orig_termios);
    atexit(disable_raw_mode);

    struct termios raw = orig_termios;
    raw.c_lflag &= ~(ECHO | ICANON);
    raw.c_cc[VMIN]  = 0;
    raw.c_cc[VTIME] = 0;

    tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw);
}

static int read_key_nonblock(void) {
    unsigned char c;
    int n = read(STDIN_FILENO, &c, 1);
    if (n == 1) return (int)c;
    return -1;
}

/* ================= Time helpers ================= */

static double now_s(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (double)ts.tv_sec + (double)ts.tv_nsec / 1e9;
}

/* ================= Net conversions (q100) ================= */

static float s16_to_f(uint16_t x_net) {
    return (float)((int16_t)ntohs(x_net)) / 100.0f;
}

static float u16_to_f(uint16_t x_net) {
    return (float)ntohs(x_net) / 100.0f;
}

static int16_t q100(float v) {
    int32_t t = (int32_t)(v * 100.0f);
    if (t < -32768) t = -32768;
    if (t >  32767) t =  32767;
    return (int16_t)t;
}

/* ================= Rendering (same ASCII layout) ================= */

#define TERM_W 80
#define TERM_H 24

static void draw_state(const NetState *ns) {
    char screen[TERM_H][TERM_W + 1];

    for (int y = 0; y < TERM_H; y++) {
        for (int x = 0; x < TERM_W; x++) screen[y][x] = ' ';
        screen[y][TERM_W] = '\0';
    }

    for (int x = 0; x < TERM_W; x++) {
        screen[0][x] = '-';
        screen[TERM_H - 1][x] = '-';
    }
    for (int y = 0; y < TERM_H; y++) {
        screen[y][0] = '|';
        screen[y][TERM_W - 1] = '|';
    }
    for (int y = 1; y < TERM_H - 1; y += 2)
        screen[y][TERM_W / 2] = '|';

    char score[32];
    snprintf(score, sizeof(score), " %u : %u ",
             ntohs(ns->score_left), ntohs(ns->score_right));
    int start = (TERM_W - (int)strlen(score)) / 2;
    for (int i = 0; score[i]; i++)
        screen[0][start + i] = score[i];

    float fw = u16_to_f(ns->field_w);
    float fh = u16_to_f(ns->field_h);
    float ph = u16_to_f(ns->paddle_h);

    float sx = (TERM_W - 2) / fw;
    float sy = (TERM_H - 2) / fh;

    int pyL = (int)(s16_to_f((uint16_t)ns->paddle_left_y) * sy);
    int pyR = (int)(s16_to_f((uint16_t)ns->paddle_right_y) * sy);
    int paddle_h = (int)(ph * sy);

    for (int i = -paddle_h / 2; i <= paddle_h / 2; i++) {
        if (pyL + i > 0 && pyL + i < TERM_H - 1) screen[pyL + i][2] = '#';
        if (pyR + i > 0 && pyR + i < TERM_H - 1) screen[pyR + i][TERM_W - 3] = '#';
    }

    int bx = (int)(s16_to_f((uint16_t)ns->ball_x) * sx);
    int by = (int)(s16_to_f((uint16_t)ns->ball_y) * sy);
    if (bx > 0 && bx < TERM_W - 1 && by > 0 && by < TERM_H - 1)
        screen[by][bx] = 'O';

    printf("\033[H\033[J");
    for (int y = 0; y < TERM_H; y++) printf("%s\n", screen[y]);
    fflush(stdout);
}

/* ================= Prediction params ================= */

// MUST match server tick and paddle speed (from game.c)
#define TICK_HZ 60
#define DT (1.0f / (float)TICK_HZ)

// If you changed g->paddle_speed in game.c, update this too
#define PADDLE_SPEED 55.0f

// If no key repeat arrives after this time, consider key released -> stop
#define RELEASE_TIMEOUT_S 0.12

// Reconciliation tuning (units are "game units")
#define DEADZONE            0.25f  // ignore tiny errors to avoid jitter
#define SNAP_THRESHOLD      6.00f  // if too far, snap to server
#define MAX_CORR_PER_FRAME  1.50f  // limit correction step per frame

static float clampf(float x, float lo, float hi) {
    if (x < lo) return lo;
    if (x > hi) return hi;
    return x;
}

static float absf(float x) { return (x < 0) ? -x : x; }

/* ================= Main ================= */

int main(int argc, char **argv) {
    if (argc != 3) {
        fprintf(stderr, "Usage: %s <ip> <port>\n", argv[0]);
        return 1;
    }

    int fd = socket(AF_INET, SOCK_STREAM, 0);
    if (fd < 0) {
        perror("socket");
        return 1;
    }

    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons((uint16_t)atoi(argv[2]));
    if (inet_pton(AF_INET, argv[1], &addr.sin_addr) != 1) {
        fprintf(stderr, "Invalid IP\n");
        close(fd);
        return 1;
    }

    if (connect(fd, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
        perror("connect");
        close(fd);
        return 1;
    }

    MsgHello hello;
    if (recv_all(fd, &hello, sizeof(hello)) <= 0 || hello.type != MSG_HELLO) {
        fprintf(stderr, "Failed to receive HELLO\n");
        close(fd);
        return 1;
    }

    uint8_t my_id = hello.player_id;
    printf("Connected as player %d (%s)\n", my_id, (my_id == 1) ? "LEFT" : "RIGHT");
    printf("Controls: W=UP, S=DOWN, Q=quit\n");
    usleep(600000);

    enable_raw_mode();

    float predicted_y = -1.0f; // init on first state
    int dir_state = 0;         // 0 none, 1 up, 2 down
    double last_dir_time = 0.0;

    while (1) {
        /* ---- read keyboard (W/S for everyone) ---- */
        int c = read_key_nonblock();
        double t = now_s();

        if (c != -1) {
            if (c == 'q' || c == 'Q') break;

            if (c == 'w' || c == 'W') {
                dir_state = 1;
                last_dir_time = t;
            } else if (c == 's' || c == 'S') {
                dir_state = 2;
                last_dir_time = t;
            }
        }

        /* emulate key release: if no repeats, stop */
        if (dir_state != 0 && (t - last_dir_time) > RELEASE_TIMEOUT_S) {
            dir_state = 0;
        }

        /* ---- send input ---- */
        MsgInput in;
        memset(&in, 0, sizeof(in));
        in.type = MSG_INPUT;
        in.player_id = my_id;
        in.dir = (uint8_t)dir_state;

        if (send_all(fd, &in, sizeof(in)) != 0) {
            fprintf(stderr, "send failed\n");
            break;
        }

        /* ---- receive authoritative state ---- */
        MsgState st;
        int rr = recv_all(fd, &st, sizeof(st));
        if (rr <= 0) {
            fprintf(stderr, "server disconnected\n");
            break;
        }
        if (st.type != MSG_STATE) continue;

        float server_y = (my_id == 1)
            ? s16_to_f((uint16_t)st.st.paddle_left_y)
            : s16_to_f((uint16_t)st.st.paddle_right_y);

        if (predicted_y < 0.0f) {
            predicted_y = server_y;
        }

        /* ---- Prediction (move immediately) ---- */
        if (dir_state == 1) predicted_y -= PADDLE_SPEED * DT;
        else if (dir_state == 2) predicted_y += PADDLE_SPEED * DT;

        /* clamp prediction inside field */
        float fh = u16_to_f(st.st.field_h);
        float ph = u16_to_f(st.st.paddle_h);
        float half_ph = ph * 0.5f;
        predicted_y = clampf(predicted_y, half_ph, fh - half_ph);

        /* ---- Improved reconciliation (less "puxão") ---- */
        float error = server_y - predicted_y;
        float aerr = absf(error);

        if (aerr > SNAP_THRESHOLD) {
            // prediction drifted too much -> snap (rare)
            predicted_y = server_y;
        } else if (aerr > DEADZONE) {
            // smooth correction but limited per frame
            // proportional step:
            float step = error * 0.35f; // stronger than 0.25 but controlled by clamp
            if (step >  MAX_CORR_PER_FRAME) step =  MAX_CORR_PER_FRAME;
            if (step < -MAX_CORR_PER_FRAME) step = -MAX_CORR_PER_FRAME;
            predicted_y += step;
        } else {
            // inside deadzone: do nothing (prevents jitter)
        }

        /* ---- Render: override ONLY your paddle with predicted_y ---- */
        if (my_id == 1) {
            st.st.paddle_left_y = htons((uint16_t)q100(predicted_y));
        } else {
            st.st.paddle_right_y = htons((uint16_t)q100(predicted_y));
        }

        draw_state(&st.st);

        usleep(1000000 / TICK_HZ);
    }

    close(fd);
    return 0;
}
