// server_tcp.c - Pong TCP server (authoritative)
// Build: gcc server_tcp.c game.c -o server_tcp -lm
// Run : ./server_tcp 5555

#include <arpa/inet.h>
#include <errno.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <unistd.h>

#include "game.h"

#define MAX_CLIENTS 2
#define TICK_HZ 60
#define TICK_US (1000000 / TICK_HZ)

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
        if (n == 0) return 0;      // disconnect
        if (n < 0) return -1;      // error
        recvd += (size_t)n;
    }
    return 1; // success
}

/* ---------- Simple binary protocol (fixed-size messages) ---------- */

enum { MSG_HELLO = 1, MSG_INPUT = 2, MSG_STATE = 3 };

/* Client -> Server */
typedef struct __attribute__((packed)) {
    uint8_t type;      // MSG_INPUT
    uint8_t player_id; // 1 or 2
    uint8_t dir;       // 0 none, 1 up, 2 down
    uint8_t _pad;
} MsgInput;

/* Server -> Client */
typedef struct __attribute__((packed)) {
    uint8_t type;      // MSG_HELLO
    uint8_t player_id; // 1 or 2
    uint16_t _pad;
} MsgHello;

/* Quantized state (avoid float endianness issues) */
typedef struct __attribute__((packed)) {
    uint16_t tick;          // wraps (network order)
    int16_t ball_x;         // x * 100
    int16_t ball_y;         // y * 100
    int16_t paddle_left_y;  // y * 100
    int16_t paddle_right_y; // y * 100
    uint16_t score_left;
    uint16_t score_right;
    uint16_t field_w;       // *100
    uint16_t field_h;       // *100
    uint16_t paddle_h;      // *100
    uint16_t ball_size;     // *100
} NetState;

typedef struct __attribute__((packed)) {
    uint8_t type; // MSG_STATE
    uint8_t _pad;
    uint16_t size; // sizeof(NetState) (network order)
    NetState st;   // all fields network order
} MsgState;

static uint16_t u16_net(uint16_t x) { return htons(x); }
static uint16_t u16_host(uint16_t x) { return ntohs(x); }

static uint16_t s16_net(int16_t x) { return htons((uint16_t)x); }
static int16_t  s16_host(uint16_t x) { return (int16_t)ntohs(x); }

static int16_t q100(float v) {
    int32_t t = (int32_t)(v * 100.0f);
    if (t < -32768) t = -32768;
    if (t >  32767) t =  32767;
    return (int16_t)t;
}

static uint16_t uq100(float v) {
    int32_t t = (int32_t)(v * 100.0f);
    if (t < 0) t = 0;
    if (t > 65535) t = 65535;
    return (uint16_t)t;
}

static void fill_netstate(NetState *ns, const GameState *g) {
    ns->tick = u16_net((uint16_t)g->tick);

    ns->ball_x = s16_net(q100(g->ball_x));
    ns->ball_y = s16_net(q100(g->ball_y));

    ns->paddle_left_y  = s16_net(q100(g->paddle_left_y));
    ns->paddle_right_y = s16_net(q100(g->paddle_right_y));

    ns->score_left  = u16_net((uint16_t)g->score_left);
    ns->score_right = u16_net((uint16_t)g->score_right);

    ns->field_w   = u16_net(uq100(g->field_w));
    ns->field_h   = u16_net(uq100(g->field_h));
    ns->paddle_h  = u16_net(uq100(g->paddle_h));
    ns->ball_size = u16_net(uq100(g->ball_size));
}

static PlayerInput dir_to_input(uint8_t dir) {
    if (dir == 1) return INPUT_UP;
    if (dir == 2) return INPUT_DOWN;
    return INPUT_NONE;
}

static int make_server_socket(uint16_t port) {
    int fd = socket(AF_INET, SOCK_STREAM, 0);
    if (fd < 0) return -1;

    int yes = 1;
    setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(yes));

    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_ANY);
    addr.sin_port = htons(port);

    if (bind(fd, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
        close(fd);
        return -1;
    }
    if (listen(fd, 8) < 0) {
        close(fd);
        return -1;
    }
    return fd;
}

static void set_nonblocking(int fd) {
    int flags = fcntl(fd, F_GETFL, 0);
    if (flags >= 0) fcntl(fd, F_SETFL, flags | O_NONBLOCK);
}

int main(int argc, char **argv) {
    if (argc != 2) {
        fprintf(stderr, "Usage: %s <port>\n", argv[0]);
        return 1;
    }
    uint16_t port = (uint16_t)atoi(argv[1]);

    int server_fd = make_server_socket(port);
    if (server_fd < 0) {
        perror("server socket");
        return 1;
    }

    printf("[server] Listening on port %u...\n", port);

    int client_fd[MAX_CLIENTS] = { -1, -1 };
    uint8_t client_id[MAX_CLIENTS] = { 1, 2 };

    // Accept exactly 2 clients
    for (int i = 0; i < MAX_CLIENTS; i++) {
        struct sockaddr_in caddr;
        socklen_t clen = sizeof(caddr);
        int cfd = accept(server_fd, (struct sockaddr *)&caddr, &clen);
        if (cfd < 0) {
            perror("accept");
            close(server_fd);
            return 1;
        }
        client_fd[i] = cfd;

        char ip[64];
        inet_ntop(AF_INET, &caddr.sin_addr, ip, sizeof(ip));
        printf("[server] Client %d connected from %s:%u\n",
               i + 1, ip, (unsigned)ntohs(caddr.sin_port));

        // Send HELLO with assigned player_id (1 for first, 2 for second)
        MsgHello hello;
        memset(&hello, 0, sizeof(hello));
        hello.type = MSG_HELLO;
        hello.player_id = client_id[i];
        if (send_all(cfd, &hello, sizeof(hello)) != 0) {
            fprintf(stderr, "[server] failed to send HELLO\n");
            close(cfd);
            close(server_fd);
            return 1;
        }

        // Non-blocking clients for input polling
        // (fcntl needs <fcntl.h>; we avoid it by using select only on blocking fds,
        // but nonblocking is still helpful. If you want: include <fcntl.h> and call set_nonblocking.)
    }

    printf("[server] Two clients connected. Starting game loop @ %d Hz\n", TICK_HZ);

    GameState g;
    game_init(&g);

    uint8_t last_dir_p1 = 0; // 0 none, 1 up, 2 down
    uint8_t last_dir_p2 = 0;

    struct timeval last_tv;
    gettimeofday(&last_tv, NULL);

    while (1) {
        // --- Poll inputs (non-blocking) ---
        fd_set rfds;
        FD_ZERO(&rfds);
        int maxfd = -1;
        for (int i = 0; i < MAX_CLIENTS; i++) {
            FD_SET(client_fd[i], &rfds);
            if (client_fd[i] > maxfd) maxfd = client_fd[i];
        }

        struct timeval tv;
        tv.tv_sec = 0;
        tv.tv_usec = 0; // pure polling
        int ready = select(maxfd + 1, &rfds, NULL, NULL, &tv);
        if (ready < 0) {
            perror("select");
            break;
        }

        for (int i = 0; i < MAX_CLIENTS; i++) {
            if (!FD_ISSET(client_fd[i], &rfds)) continue;

            MsgInput in;
            int r = recv_all(client_fd[i], &in, sizeof(in));
            if (r == 0) {
                printf("[server] client %d disconnected\n", i + 1);
                goto shutdown;
            }
            if (r < 0) {
                // if it's EAGAIN in real nonblocking, you'd ignore. Here recv_all blocks.
                perror("recv");
                goto shutdown;
            }

            if (in.type != MSG_INPUT) continue;
            if (in.player_id == 1) last_dir_p1 = in.dir;
            if (in.player_id == 2) last_dir_p2 = in.dir;
        }

        // --- Advance game @ fixed dt ---
        PlayerInput p1 = dir_to_input(last_dir_p1);
        PlayerInput p2 = dir_to_input(last_dir_p2);

        game_step(&g, p1, p2);

        // --- Broadcast state ---
        MsgState out;
        memset(&out, 0, sizeof(out));
        out.type = MSG_STATE;
        out.size = u16_net((uint16_t)sizeof(NetState));
        fill_netstate(&out.st, &g);

        for (int i = 0; i < MAX_CLIENTS; i++) {
            if (send_all(client_fd[i], &out, sizeof(out)) != 0) {
                printf("[server] send failed, client %d\n", i + 1);
                goto shutdown;
            }
        }

        // --- Sleep to maintain tick rate ---
        usleep(TICK_US);
    }

shutdown:
    for (int i = 0; i < MAX_CLIENTS; i++) {
        if (client_fd[i] >= 0) close(client_fd[i]);
    }
    close(server_fd);
    return 0;
}
