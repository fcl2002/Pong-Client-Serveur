// client_tcp.c - Pong TCP client (W/S and 8/2 controls, fixed render)

#include <arpa/inet.h>
#include <netinet/in.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <termios.h>
#include <unistd.h>

/* ================= Protocol ================= */

enum { MSG_HELLO = 1, MSG_INPUT = 2, MSG_STATE = 3 };

typedef struct __attribute__((packed)) {
    uint8_t type;
    uint8_t player_id;
    uint8_t dir;   // 0 none, 1 up, 2 down
    uint8_t _pad;
} MsgInput;

typedef struct __attribute__((packed)) {
    uint8_t type;
    uint8_t player_id;
    uint16_t _pad;
} MsgHello;

typedef struct __attribute__((packed)) {
    uint16_t tick;
    int16_t ball_x;
    int16_t ball_y;
    int16_t paddle_left_y;
    int16_t paddle_right_y;
    uint16_t score_left;
    uint16_t score_right;
    uint16_t field_w;
    uint16_t field_h;
    uint16_t paddle_h;
    uint16_t ball_size;
} NetState;

typedef struct __attribute__((packed)) {
    uint8_t type;
    uint8_t _pad;
    uint16_t size;
    NetState st;
} MsgState;

/* ================= Utils ================= */

static int send_all(int fd, const void *buf, size_t len) {
    const uint8_t *p = buf;
    size_t sent = 0;
    while (sent < len) {
        ssize_t n = send(fd, p + sent, len - sent, 0);
        if (n <= 0) return -1;
        sent += (size_t)n;
    }
    return 0;
}

static int recv_all(int fd, void *buf, size_t len) {
    uint8_t *p = buf;
    size_t recvd = 0;
    while (recvd < len) {
        ssize_t n = recv(fd, p + recvd, len - recvd, 0);
        if (n <= 0) return n;
        recvd += (size_t)n;
    }
    return 1;
}

static float s16_to_f(uint16_t x) { return (int16_t)ntohs(x) / 100.0f; }
static float u16_to_f(uint16_t x) { return ntohs(x) / 100.0f; }

/* ================= Terminal ================= */

static struct termios orig_termios;

static void disable_raw_mode(void) {
    tcsetattr(STDIN_FILENO, TCSAFLUSH, &orig_termios);
}

static void enable_raw_mode(void) {
    tcgetattr(STDIN_FILENO, &orig_termios);
    atexit(disable_raw_mode);

    struct termios raw = orig_termios;
    raw.c_lflag &= ~(ECHO | ICANON);
    raw.c_cc[VMIN] = 0;
    raw.c_cc[VTIME] = 0;
    tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw);
}

static int read_key_nonblock(void) {
    unsigned char c;
    int n = read(STDIN_FILENO, &c, 1);
    if (n == 1) return c;
    return -1;
}

/* ================= Rendering ================= */

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

    int pyL = (int)(s16_to_f(ns->paddle_left_y) * sy);
    int pyR = (int)(s16_to_f(ns->paddle_right_y) * sy);
    int paddle_h = (int)(ph * sy);

    for (int i = -paddle_h / 2; i <= paddle_h / 2; i++) {
        if (pyL + i > 0 && pyL + i < TERM_H - 1) screen[pyL + i][2] = '#';
        if (pyR + i > 0 && pyR + i < TERM_H - 1) screen[pyR + i][TERM_W - 3] = '#';
    }

    int bx = (int)(s16_to_f(ns->ball_x) * sx);
    int by = (int)(s16_to_f(ns->ball_y) * sy);
    if (bx > 0 && bx < TERM_W - 1 && by > 0 && by < TERM_H - 1)
        screen[by][bx] = 'O';

    printf("\033[H\033[J");
    for (int y = 0; y < TERM_H; y++) printf("%s\n", screen[y]);
    fflush(stdout);
}

/* ================= Main ================= */

int main(int argc, char **argv) {
    if (argc != 3) {
        fprintf(stderr, "Usage: %s <ip> <port>\n", argv[0]);
        return 1;
    }

    int fd = socket(AF_INET, SOCK_STREAM, 0);

    struct sockaddr_in addr = {0};
    addr.sin_family = AF_INET;
    addr.sin_port = htons(atoi(argv[2]));
    inet_pton(AF_INET, argv[1], &addr.sin_addr);

    connect(fd, (struct sockaddr *)&addr, sizeof(addr));

    MsgHello hello;
    recv_all(fd, &hello, sizeof(hello));

    uint8_t my_id = hello.player_id;
    printf("Connected as player %d (%s)\n",
           my_id, my_id == 1 ? "LEFT" : "RIGHT");
    usleep(500000);

    enable_raw_mode();

    uint8_t dir = 0;

    while (1) {

        uint8_t dir = 0;
        int c = read_key_nonblock();
        
        if (c != -1) {
            if (c == 'q' || c == 'Q') break;

            if (c == 'w' || c == 'W')
                dir = 1;          /* UP */
            else if (c == 's' || c == 'S')
                dir = 2;          /* DOWN */
            else
                dir = 0;
        } else {
            dir = 0;
        }

        MsgInput in = { MSG_INPUT, my_id, dir, 0 };
        send_all(fd, &in, sizeof(in));

        MsgState st;
        if (recv_all(fd, &st, sizeof(st)) <= 0) break;

        draw_state(&st.st);
        usleep(1000000 / 60);
    }

    close(fd);
    return 0;
}
