/* game.h - Pong core (no networking), server-authoritative style */
#ifndef GAME_H
#define GAME_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>

/* Possible inputs per player (the client would send this to the server) */
typedef enum {
    INPUT_NONE = 0,
    INPUT_UP   = 1,
    INPUT_DOWN = 2
} PlayerInput;

/* Game state (maintained by the server) */
typedef struct {
    /* Logical field dimensions */
    float field_w;
    float field_h;

    /* Paddles */
    float paddle_h;      /* paddle height */
    float paddle_w;      /* paddle width (for collision) */
    float paddle_speed;  /* units per second */

    float paddle_left_y;   /* left paddle center (y) */
    float paddle_right_y;  /* right paddle center (y) */

    /* Ball */
    float ball_x;
    float ball_y;
    float ball_vx;
    float ball_vy;
    float ball_size;     /* "radius" or half-size of the square (for simple collision) */

    /* Score */
    int score_left;
    int score_right;

    /* Time control */
    uint32_t tick;       /* number of ticks since start */
    float dt;            /* seconds per tick (e.g. 1/60) */

    /* Serve / pause between points */
    uint32_t serve_pause_ticks; /* how many ticks to wait after a point */
    uint32_t serve_wait;        /* remaining wait ticks */

    /* "Atari-like" configuration */
    float ball_speed_base;   /* initial ball speed */
    float ball_speed_max;    /* speed limit */
    float ball_speed_gain;   /* multiplier per paddle hit (e.g. 1.05) */
    float min_vy_abs;        /* prevents the ball from going too straight forever */

} GameState;

/* Initialize with default parameters and reset the round */
void game_init(GameState *g);

/* Reset the ball to the center and apply serve pause.
   serve_dir: -1 (to the left), +1 (to the right). */
void game_reset_round(GameState *g, int serve_dir);

/* Advance the game by one tick, applying the inputs of the current tick */
void game_step(GameState *g, PlayerInput left_in, PlayerInput right_in);

#ifdef __cplusplus
}
#endif

#endif /* GAME_H */
