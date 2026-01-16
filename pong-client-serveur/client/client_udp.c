/* client_udp.c - Pong UDP Client with ASCII rendering */
#include "../server/game.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/time.h>
#include <errno.h>
#include <termios.h>
#include <fcntl.h>
#include <signal.h>

#define SERVER_PORT 12345
#define BUFFER_SIZE 1024
#define KEEPALIVE_INTERVAL_MS 1000
#define RENDER_WIDTH 80
#define RENDER_HEIGHT 24

/* Protocol message types */
typedef enum {
    MSG_CLIENT_CONNECT = 1,
    MSG_CLIENT_INPUT = 2,
    MSG_SERVER_STATE = 3,
    MSG_CLIENT_DISCONNECT = 4
} MessageType;

/* Message structures */
typedef struct {
    uint8_t type;
    uint8_t player_id;
} __attribute__((packed)) ConnectMsg;

typedef struct {
    uint8_t type;
    uint8_t player_id;
    uint8_t input;
} __attribute__((packed)) InputMsg;

typedef struct {
    uint8_t type;
    float ball_x;
    float ball_y;
    float paddle_left_y;
    float paddle_right_y;
    int score_left;
    int score_right;
    uint32_t tick;
    uint8_t player0_connected;
    uint8_t player1_connected;
} __attribute__((packed)) StateMsg;

/* Client state */
typedef struct {
    int sockfd;
    struct sockaddr_in server_addr;
    int player_id;
    PlayerInput current_input;
    StateMsg last_state;
    int connected;
    uint64_t last_keepalive_ms;
} ClientState;

/* Terminal settings for raw input */
static struct termios original_termios;
static int terminal_configured = 0;

/* Get current time in milliseconds */
static uint64_t get_time_ms(void) {
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return (uint64_t)tv.tv_sec * 1000 + tv.tv_usec / 1000;
}

/* Restore terminal settings */
static void restore_terminal(void) {
    if (terminal_configured) {
        tcsetattr(STDIN_FILENO, TCSANOW, &original_termios);
        terminal_configured = 0;
    }
}

/* Cleanup on exit */
static void cleanup_handler(int sig) {
    restore_terminal();
    printf("\n\nDisconnected from server.\n");
    exit(0);
}

/* Configure terminal for non-blocking raw input */
static void configure_terminal(void) {
    struct termios raw;
    
    /* Save original settings */
    tcgetattr(STDIN_FILENO, &original_termios);
    terminal_configured = 1;
    
    /* Set signal handler */
    signal(SIGINT, cleanup_handler);
    signal(SIGTERM, cleanup_handler);
    
    /* Configure raw mode */
    raw = original_termios;
    raw.c_lflag &= ~(ECHO | ICANON);
    raw.c_cc[VMIN] = 0;
    raw.c_cc[VTIME] = 0;
    tcsetattr(STDIN_FILENO, TCSANOW, &raw);
    
    /* Set stdin to non-blocking */
    int flags = fcntl(STDIN_FILENO, F_GETFL, 0);
    fcntl(STDIN_FILENO, F_SETFL, flags | O_NONBLOCK);
}

/* Send connect message */
static void send_connect(ClientState *client) {
    ConnectMsg msg;
    msg.type = MSG_CLIENT_CONNECT;
    msg.player_id = client->player_id;
    
    sendto(client->sockfd, &msg, sizeof(msg), 0,
           (struct sockaddr *)&client->server_addr,
           sizeof(client->server_addr));
}

/* Send input message */
static void send_input(ClientState *client) {
    InputMsg msg;
    msg.type = MSG_CLIENT_INPUT;
    msg.player_id = client->player_id;
    msg.input = client->current_input;
    
    sendto(client->sockfd, &msg, sizeof(msg), 0,
           (struct sockaddr *)&client->server_addr,
           sizeof(client->server_addr));
}

/* Send disconnect message */
static void send_disconnect(ClientState *client) {
    uint8_t msg = MSG_CLIENT_DISCONNECT;
    
    sendto(client->sockfd, &msg, sizeof(msg), 0,
           (struct sockaddr *)&client->server_addr,
           sizeof(client->server_addr));
}

/* Read keyboard input (non-blocking) */
static PlayerInput read_input(int *quit_flag) {
    char c;
    if (read(STDIN_FILENO, &c, 1) == 1) {
        switch (c) {
            case 'w': case 'W': return INPUT_UP;
            case 's': case 'S': return INPUT_DOWN;
            case 'q': case 'Q': 
                *quit_flag = 1;
                return INPUT_NONE;
        }
    }
    return INPUT_NONE;
}

/* Map game coordinates to screen coordinates */
static void map_to_screen(float game_x, float game_y,
                         float field_w, float field_h,
                         int *screen_x, int *screen_y) {
    *screen_x = (int)((game_x / field_w) * (RENDER_WIDTH - 2)) + 1;
    *screen_y = (int)((game_y / field_h) * (RENDER_HEIGHT - 2)) + 1;
    
    if (*screen_x < 1) *screen_x = 1;
    if (*screen_x >= RENDER_WIDTH - 1) *screen_x = RENDER_WIDTH - 2;
    if (*screen_y < 1) *screen_y = 1;
    if (*screen_y >= RENDER_HEIGHT - 1) *screen_y = RENDER_HEIGHT - 2;
}

/* Render game state as ASCII */
static void render_state(ClientState *client) {
    char screen[RENDER_HEIGHT][RENDER_WIDTH + 1];
    
    /* Clear screen buffer */
    for (int y = 0; y < RENDER_HEIGHT; y++) {
        for (int x = 0; x < RENDER_WIDTH; x++) {
            if (y == 0 || y == RENDER_HEIGHT - 1) {
                screen[y][x] = '-';
            } else if (x == 0 || x == RENDER_WIDTH - 1) {
                screen[y][x] = '|';
            } else if (x == RENDER_WIDTH / 2) {
                screen[y][x] = ':'; /* center line */
            } else {
                screen[y][x] = ' ';
            }
        }
        screen[y][RENDER_WIDTH] = '\0';
    }
    
    StateMsg *state = &client->last_state;
    
    /* Assume field dimensions from game.c defaults */
    float field_w = 100.0f;
    float field_h = 60.0f;
    float paddle_h = 14.0f;
    
    /* Draw left paddle */
    int px, py;
    map_to_screen(3.5f, state->paddle_left_y, field_w, field_h, &px, &py);
    int paddle_screen_h = (int)((paddle_h / field_h) * (RENDER_HEIGHT - 2));
    if (paddle_screen_h < 3) paddle_screen_h = 3;
    
    for (int i = -paddle_screen_h/2; i <= paddle_screen_h/2; i++) {
        int draw_y = py + i;
        if (draw_y > 0 && draw_y < RENDER_HEIGHT - 1) {
            screen[draw_y][px] = '|';
        }
    }
    
    /* Draw right paddle */
    map_to_screen(field_w - 3.5f, state->paddle_right_y, field_w, field_h, &px, &py);
    for (int i = -paddle_screen_h/2; i <= paddle_screen_h/2; i++) {
        int draw_y = py + i;
        if (draw_y > 0 && draw_y < RENDER_HEIGHT - 1) {
            screen[draw_y][px] = '|';
        }
    }
    
    /* Draw ball */
    int bx, by;
    map_to_screen(state->ball_x, state->ball_y, field_w, field_h, &bx, &by);
    if (bx > 0 && bx < RENDER_WIDTH - 1 && by > 0 && by < RENDER_HEIGHT - 1) {
        screen[by][bx] = 'O';
    }
    
    /* Clear screen and render */
    printf("\033[2J\033[H"); /* ANSI: clear screen and move cursor to top-left */
    
    printf("PONG - Player %d\n", client->player_id + 1);
    printf("Score: %d - %d\n", state->score_left, state->score_right);
    
    /* Show connection status */
    if (!state->player0_connected || !state->player1_connected) {
        printf("[");
        if (!state->player0_connected && !state->player1_connected) {
            printf("Both players disconnected");
        } else if (!state->player0_connected) {
            printf("Player 1 disconnected");
        } else if (!state->player1_connected) {
            printf("Player 2 disconnected");
        }
        printf(" - Waiting for reconnection...]\n");
    } else {
        printf("\n");
    }
    printf("\n");
    
    for (int y = 0; y < RENDER_HEIGHT; y++) {
        printf("%s\n", screen[y]);
    }
    
    printf("\nControls: W/S to move | Q to quit\n");
    
    fflush(stdout);
}

/* Initialize client */
static int client_init(ClientState *client, const char *server_ip, int player_id) {
    memset(client, 0, sizeof(ClientState));
    client->player_id = player_id;
    client->current_input = INPUT_NONE;
    client->connected = 0;
    
    /* Create UDP socket */
    client->sockfd = socket(AF_INET, SOCK_DGRAM, 0);
    if (client->sockfd < 0) {
        perror("socket creation failed");
        return -1;
    }
    
    /* Set socket to non-blocking */
    struct timeval tv;
    tv.tv_sec = 0;
    tv.tv_usec = 1000; /* 1ms timeout */
    setsockopt(client->sockfd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));
    
    /* Configure server address */
    memset(&client->server_addr, 0, sizeof(client->server_addr));
    client->server_addr.sin_family = AF_INET;
    client->server_addr.sin_port = htons(SERVER_PORT);
    
    if (inet_pton(AF_INET, server_ip, &client->server_addr.sin_addr) <= 0) {
        perror("invalid server address");
        close(client->sockfd);
        return -1;
    }
    
    return 0;
}

int main(int argc, char *argv[]) {
    ClientState client;
    const char *server_ip = "127.0.0.1";
    int player_id = 0;
    
    /* Parse command line arguments */
    if (argc >= 2) {
        server_ip = argv[1];
    }
    if (argc >= 3) {
        player_id = atoi(argv[2]);
        if (player_id < 0 || player_id > 1) {
            printf("Player ID must be 0 or 1\n");
            return 1;
        }
    }
    
    /* Initialize client */
    if (client_init(&client, server_ip, player_id) < 0) {
        return 1;
    }
    
    /* Configure terminal */
    configure_terminal();
    
    printf("Connecting to server %s:%d as Player %d...\n",
           server_ip, SERVER_PORT, player_id + 1);
    
    /* Send initial connect message */
    send_connect(&client);
    client.last_keepalive_ms = get_time_ms();
    
    uint8_t buffer[BUFFER_SIZE];
    int running = 1;
    int quit_pressed = 0;
    
    /* Main client loop */
    while (running) {
        uint64_t now = get_time_ms();
        
        /* Read keyboard input */
        PlayerInput new_input = read_input(&quit_pressed);
        
        /* Check if quit was pressed */
        if (quit_pressed) {
            running = 0;
            break;
        }
        
        /* Update input if changed */
        if (new_input != client.current_input) {
            client.current_input = new_input;
            send_input(&client);
        }
        
        /* Send keepalive/input periodically */
        if (now - client.last_keepalive_ms >= KEEPALIVE_INTERVAL_MS) {
            send_input(&client);
            client.last_keepalive_ms = now;
        }
        
        /* Receive state updates from server */
        struct sockaddr_in from_addr;
        socklen_t from_len = sizeof(from_addr);
        int recv_len = recvfrom(client.sockfd, buffer, BUFFER_SIZE, 0,
                               (struct sockaddr *)&from_addr, &from_len);
        
        if (recv_len >= sizeof(StateMsg)) {
            StateMsg *msg = (StateMsg *)buffer;
            if (msg->type == MSG_SERVER_STATE) {
                client.last_state = *msg;
                client.connected = 1;
                
                /* Render the game */
                render_state(&client);
            }
        }
        
        usleep(16000); /* ~60 FPS rendering */
    }
    
    /* Cleanup */
    send_disconnect(&client);
    close(client.sockfd);
    restore_terminal();
    
    printf("\nDisconnected from server.\n");
    
    return 0;
}