/* server_udp.c - Pong UDP Server */
#include "game.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/time.h>
#include <errno.h>

#define SERVER_PORT 12345
#define BUFFER_SIZE 1024
#define TICK_INTERVAL_MS 16  /* ~60 Hz (16.67 ms) */
#define CLIENT_TIMEOUT_MS 5000
#define MAX_CLIENTS 2

/* Protocol message types */
typedef enum {
    MSG_CLIENT_CONNECT = 1,
    MSG_CLIENT_INPUT = 2,
    MSG_SERVER_STATE = 3,
    MSG_CLIENT_DISCONNECT = 4
} MessageType;

/* Client connection info */
typedef struct {
    struct sockaddr_in addr;
    socklen_t addr_len;
    int active;
    uint64_t last_seen_ms;
    PlayerInput current_input;
} ClientInfo;

/* Message structures */
typedef struct {
    uint8_t type;
    uint8_t player_id;
} __attribute__((packed)) ConnectMsg;

typedef struct {
    uint8_t type;
    uint8_t player_id;
    uint8_t input;  /* PlayerInput enum */
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
    uint8_t player0_connected;  /* 1 if player 0 is active, 0 otherwise */
    uint8_t player1_connected;  /* 1 if player 1 is active, 0 otherwise */
} __attribute__((packed)) StateMsg;

/* Get current time in milliseconds */
static uint64_t get_time_ms(void) {
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return (uint64_t)tv.tv_sec * 1000 + tv.tv_usec / 1000;
}

/* Check if two sockaddr_in are equal */
static int addr_equal(struct sockaddr_in *a, struct sockaddr_in *b) {
    return (a->sin_addr.s_addr == b->sin_addr.s_addr &&
            a->sin_port == b->sin_port);
}

/* Forward declarations */
static void broadcast_state(int sockfd, ClientInfo clients[], GameState *game);

/* Find client by address */
static int find_client(ClientInfo clients[], struct sockaddr_in *addr) {
    for (int i = 0; i < MAX_CLIENTS; i++) {
        if (clients[i].active && addr_equal(&clients[i].addr, addr)) {
            return i;
        }
    }
    return -1;
}

/* Find free slot for new client */
static int find_free_slot(ClientInfo clients[]) {
    for (int i = 0; i < MAX_CLIENTS; i++) {
        if (!clients[i].active) {
            return i;
        }
    }
    return -1;
}

/* Handle incoming messages */
static void handle_message(int sockfd, ClientInfo clients[], 
                          uint8_t *buffer, int recv_len,
                          struct sockaddr_in *client_addr,
                          int *game_started, GameState *game) {
    uint64_t now = get_time_ms();
    
    if (recv_len < 1) return;
    
    uint8_t msg_type = buffer[0];
    
    switch (msg_type) {
        case MSG_CLIENT_CONNECT: {
            if (recv_len < sizeof(ConnectMsg)) break;
            
            int client_id = find_client(clients, client_addr);
            
            if (client_id == -1) {
                /* New client */
                client_id = find_free_slot(clients);
                if (client_id == -1) {
                    printf("Server full, rejecting connection from %s:%d\n",
                           inet_ntoa(client_addr->sin_addr),
                           ntohs(client_addr->sin_port));
                    break;
                }
                
                clients[client_id].addr = *client_addr;
                clients[client_id].addr_len = sizeof(struct sockaddr_in);
                clients[client_id].active = 1;
                clients[client_id].last_seen_ms = now;
                clients[client_id].current_input = INPUT_NONE;
                
                printf("Player %d connected: %s:%d\n",
                       client_id,
                       inet_ntoa(client_addr->sin_addr),
                       ntohs(client_addr->sin_port));
                
                /* Check if we should start the game */
                if (!(*game_started) && clients[0].active && clients[1].active) {
                    *game_started = 1;
                    printf("Both players connected! Game starting...\n");
                }
            } else {
                /* Already connected, just update timestamp */
                clients[client_id].last_seen_ms = now;
            }
            break;
        }
        
        case MSG_CLIENT_INPUT: {
            if (recv_len < sizeof(InputMsg)) break;
            
            InputMsg *msg = (InputMsg *)buffer;
            int client_id = find_client(clients, client_addr);
            
            if (client_id != -1) {
                clients[client_id].current_input = (PlayerInput)msg->input;
                clients[client_id].last_seen_ms = now;
            }
            break;
        }
        
        case MSG_CLIENT_DISCONNECT: {
            int client_id = find_client(clients, client_addr);
            if (client_id != -1) {
                printf("Player %d disconnected\n", client_id);
                clients[client_id].active = 0;
                *game_started = 0;  /* Reset game when a player leaves */
                
                /* Immediately broadcast the new state so remaining player sees disconnection */
                broadcast_state(sockfd, clients, game);
            }
            break;
        }
    }
}

/* Broadcast game state to all active clients */
static void broadcast_state(int sockfd, ClientInfo clients[], GameState *game) {
    StateMsg msg;
    msg.type = MSG_SERVER_STATE;
    msg.ball_x = game->ball_x;
    msg.ball_y = game->ball_y;
    msg.paddle_left_y = game->paddle_left_y;
    msg.paddle_right_y = game->paddle_right_y;
    msg.score_left = game->score_left;
    msg.score_right = game->score_right;
    msg.tick = game->tick;
    msg.player0_connected = clients[0].active ? 1 : 0;
    msg.player1_connected = clients[1].active ? 1 : 0;
    
    for (int i = 0; i < MAX_CLIENTS; i++) {
        if (clients[i].active) {
            sendto(sockfd, &msg, sizeof(msg), 0,
                   (struct sockaddr *)&clients[i].addr,
                   clients[i].addr_len);
        }
    }
}

/* Check for client timeouts */
static int check_timeouts(ClientInfo clients[], int *game_started) {
    uint64_t now = get_time_ms();
    int timeout_occurred = 0;
    
    for (int i = 0; i < MAX_CLIENTS; i++) {
        if (clients[i].active) {
            if (now - clients[i].last_seen_ms > CLIENT_TIMEOUT_MS) {
                printf("Player %d timed out\n", i);
                clients[i].active = 0;
                *game_started = 0;  /* Stop game when a player times out */
                timeout_occurred = 1;
            }
        }
    }
    
    return timeout_occurred;
}

int main(void) {
    int sockfd;
    struct sockaddr_in server_addr, client_addr;
    socklen_t client_len;
    uint8_t buffer[BUFFER_SIZE];
    
    GameState game;
    ClientInfo clients[MAX_CLIENTS] = {0};
    int game_started = 0;  /* Game only starts when both players connect */
    
    /* Initialize game structure */
    game_init(&game);
    
    /* Create UDP socket */
    sockfd = socket(AF_INET, SOCK_DGRAM, 0);
    if (sockfd < 0) {
        perror("socket creation failed");
        exit(EXIT_FAILURE);
    }
    
    /* Set socket to non-blocking mode */
    struct timeval tv;
    tv.tv_sec = 0;
    tv.tv_usec = 1000; /* 1ms timeout for recvfrom */
    setsockopt(sockfd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));
    
    /* Configure server address */
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY;
    server_addr.sin_port = htons(SERVER_PORT);
    
    /* Bind socket */
    if (bind(sockfd, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
        perror("bind failed");
        close(sockfd);
        exit(EXIT_FAILURE);
    }
    
    printf("Pong server started on port %d\n", SERVER_PORT);
    printf("Waiting for players...\n");
    
    uint64_t last_tick_ms = get_time_ms();
    uint64_t last_waiting_broadcast_ms = get_time_ms();
    
    /* Main game loop */
    while (1) {
        uint64_t now = get_time_ms();
        
        /* Process incoming messages (non-blocking) */
        while (1) {
            client_len = sizeof(client_addr);
            int recv_len = recvfrom(sockfd, buffer, BUFFER_SIZE, 0,
                                   (struct sockaddr *)&client_addr, &client_len);
            
            if (recv_len < 0) {
                if (errno == EAGAIN || errno == EWOULDBLOCK) {
                    break; /* No more messages */
                }
                perror("recvfrom error");
                break;
            }
            
            handle_message(sockfd, clients, buffer, recv_len, &client_addr, &game_started, &game);
        }
        
        /* Game tick update - only if game has started */
        if (game_started && now - last_tick_ms >= TICK_INTERVAL_MS) {
            last_tick_ms = now;
            
            /* Get inputs from both players */
            PlayerInput left_input = INPUT_NONE;
            PlayerInput right_input = INPUT_NONE;
            
            if (clients[0].active) left_input = clients[0].current_input;
            if (clients[1].active) right_input = clients[1].current_input;
            
            /* Step game simulation */
            game_step(&game, left_input, right_input);
            
            /* Broadcast state to clients */
            broadcast_state(sockfd, clients, &game);
            
            /* Check for timeouts */
            int timeout_occurred = check_timeouts(clients, &game_started);
            
            /* If timeout occurred, send immediate update */
            if (timeout_occurred) {
                broadcast_state(sockfd, clients, &game);
            }
        }
        
        /* When game is not running, broadcast at lower rate (10 Hz) so clients see disconnection */
        if (!game_started && (clients[0].active || clients[1].active)) {
            if (now - last_waiting_broadcast_ms >= 100) { /* 100ms = 10 Hz */
                broadcast_state(sockfd, clients, &game);
                last_waiting_broadcast_ms = now;
            }
        }
        
        /* Small sleep to prevent CPU spinning */
        usleep(1000); /* 1ms */
    }
    
    close(sockfd);
    return 0;
}