/* game.c - Pong core (no networking), server-authoritative style */
#include "game.h"
#include <math.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

/* ---------- Internal helpers ---------- */

static float clampf(float x, float lo, float hi) {
    if (x < lo) return lo;
    if (x > hi) return hi;
    return x;
}

/* Simple deterministic choice (no rand()):
   alternates a small vertical offset depending on the tick. */
static float pseudo_offset(uint32_t tick) {
    /* small values to avoid chaotic behavior */
    switch (tick % 4u) {
        case 0: return -0.25f;
        case 1: return  0.25f;
        case 2: return -0.15f;
        default:return  0.15f;
    }
}

/* Applies Atari-like rule: angle depends on where the ball hits the paddle.
   rel in [-1, +1] (top = -1, center = 0, bottom = +1).
   Computes (vx, vy) with vx pointing away from the paddle. */
static void reflect_on_paddle(GameState *g, int hit_left_paddle, float rel) {
    /* Typical zones (5 zones): -45, -20, 0, +20, +45 degrees */
    const float degs[5] = { -45.f, -20.f, 0.f, 20.f, 45.f };

    /* map rel [-1..1] -> index 0..4 */
    float t = (rel + 1.0f) * 0.5f;         /* 0..1 */
    int idx = (int)floorf(t * 5.0f);       /* 0..5 */
    if (idx < 0) idx = 0;
    if (idx > 4) idx = 4;

    float angle_deg = degs[idx];
    float angle = angle_deg * (float)M_PI / 180.0f;

    /* current speed (magnitude) */
    float speed = sqrtf(g->ball_vx * g->ball_vx + g->ball_vy * g->ball_vy);

    /* accelerate on paddle hit */
    speed *= g->ball_speed_gain;
    if (speed > g->ball_speed_max) speed = g->ball_speed_max;

    /* base direction */
    float vx = cosf(angle) * speed;
    float vy = sinf(angle) * speed;

    if (hit_left_paddle) {
        /* left paddle -> ball goes to the right (vx > 0) */
        if (vx < 0) vx = -vx;
    } else {
        /* right paddle -> ball goes to the left (vx < 0) */
        if (vx > 0) vx = -vx;
    }

    /* avoid too small vy (ball going straight forever) */
    if (fabsf(vy) < g->min_vy_abs) {
        vy = (vy < 0 ? -1.0f : 1.0f) * g->min_vy_abs;
    }

    g->ball_vx = vx;
    g->ball_vy = vy;
}

/* Checks ball (simple AABB) collision with paddle.
   Paddle has fixed x (left or right) and center y.
   Returns 1 if collided; also computes rel in [-1..1]. */
static int collide_paddle(const GameState *g,
                          float paddle_x_center,
                          float paddle_y_center,
                          float *out_rel)
{
    /* Paddle AABB */
    float px0 = paddle_x_center - g->paddle_w * 0.5f;
    float px1 = paddle_x_center + g->paddle_w * 0.5f;
    float py0 = paddle_y_center - g->paddle_h * 0.5f;
    float py1 = paddle_y_center + g->paddle_h * 0.5f;

    /* Ball AABB */
    float bx0 = g->ball_x - g->ball_size;
    float bx1 = g->ball_x + g->ball_size;
    float by0 = g->ball_y - g->ball_size;
    float by1 = g->ball_y + g->ball_size;

    int overlap = (bx1 >= px0 && bx0 <= px1 && by1 >= py0 && by0 <= py1);
    if (!overlap) return 0;

    /* rel based on vertical impact point */
    float rel = (g->ball_y - paddle_y_center) / (g->paddle_h * 0.5f);
    rel = clampf(rel, -1.0f, 1.0f);
    if (out_rel) *out_rel = rel;
    return 1;
}

/* ---------- Public API ---------- */

void game_reset_round(GameState *g, int serve_dir) {
    /* Ball to center */
    g->ball_x = g->field_w * 0.5f;
    g->ball_y = g->field_h * 0.5f;

    /* Base speed with small deterministic offset */
    float off = pseudo_offset(g->tick);
    float speed = g->ball_speed_base;

    /* Direction: serve_dir -1 = left, +1 = right */
    float vx = (serve_dir >= 0) ? speed : -speed;
    float vy = off * speed; /* small vertical component */

    /* ensure minimum vy */
    if (fabsf(vy) < g->min_vy_abs)
        vy = (vy < 0 ? -1.0f : 1.0f) * g->min_vy_abs;

    g->ball_vx = vx;
    g->ball_vy = vy;

    /* Serve pause */
    g->serve_wait = g->serve_pause_ticks;
}

void game_init(GameState *g) {
    if (!g) return;

    /* Logical field (can be mapped later to ASCII) */
    g->field_w = 100.0f;
    g->field_h = 60.0f;

    /* Paddles */
    g->paddle_h = 14.0f;
    g->paddle_w = 2.5f;
    g->paddle_speed = 55.0f; /* units/sec */

    g->paddle_left_y  = g->field_h * 0.5f;
    g->paddle_right_y = g->field_h * 0.5f;

    /* Ball */
    g->ball_size = 1.2f;

    /* Score */
    g->score_left = 0;
    g->score_right = 0;

    /* Time */
    g->tick = 0;
    g->dt = 1.0f / 60.0f; /* 60 Hz */

    /* Serve */
    g->serve_pause_ticks = 60; /* ~1 second */
    g->serve_wait = 0;

    /* Atari feeling */
    g->ball_speed_base = 45.0f;
    g->ball_speed_max  = 95.0f;
    g->ball_speed_gain = 1.05f;
    g->min_vy_abs      = 6.0f;

    /* Start serving to the right */
    game_reset_round(g, +1);
}

void game_step(GameState *g, PlayerInput left_in, PlayerInput right_in) {
    if (!g) return;

    g->tick++;

    /* If in serve pause, just decrement timer (no ball movement) */
    if (g->serve_wait > 0) {
        g->serve_wait--;
    }

    /* 1) Update paddles (always responsive, even during pause) */
    float dy_left = 0.0f;
    float dy_right = 0.0f;

    if (left_in == INPUT_UP) dy_left = -g->paddle_speed * g->dt;
    else if (left_in == INPUT_DOWN) dy_left = +g->paddle_speed * g->dt;

    if (right_in == INPUT_UP) dy_right = -g->paddle_speed * g->dt;
    else if (right_in == INPUT_DOWN) dy_right = +g->paddle_speed * g->dt;

    g->paddle_left_y  += dy_left;
    g->paddle_right_y += dy_right;

    /* clamp paddles inside the field */
    float half_ph = g->paddle_h * 0.5f;
    g->paddle_left_y  = clampf(g->paddle_left_y,  half_ph, g->field_h - half_ph);
    g->paddle_right_y = clampf(g->paddle_right_y, half_ph, g->field_h - half_ph);

    /* 2) If still in pause, do not move the ball */
    if (g->serve_wait > 0) return;

    /* 3) Move ball */
    g->ball_x += g->ball_vx * g->dt;
    g->ball_y += g->ball_vy * g->dt;

    /* 4) Collision with top/bottom walls */
    if (g->ball_y - g->ball_size <= 0.0f) {
        g->ball_y = g->ball_size;
        g->ball_vy = -g->ball_vy;
    } else if (g->ball_y + g->ball_size >= g->field_h) {
        g->ball_y = g->field_h - g->ball_size;
        g->ball_vy = -g->ball_vy;
    }

    /* 5) Collision with paddles (fixed x near borders) */
    float paddle_left_x  = 3.5f;                 /* margin */
    float paddle_right_x = g->field_w - 3.5f;

    /* Only check collision with the paddle the ball is moving towards */
    if (g->ball_vx < 0) {
        float rel = 0.0f;
        if (collide_paddle(g, paddle_left_x, g->paddle_left_y, &rel)) {
            /* push ball out of paddle to avoid sticking */
            g->ball_x = paddle_left_x + (g->paddle_w * 0.5f) + g->ball_size + 0.01f;
            reflect_on_paddle(g, 1, rel);
        }
    } else if (g->ball_vx > 0) {
        float rel = 0.0f;
        if (collide_paddle(g, paddle_right_x, g->paddle_right_y, &rel)) {
            g->ball_x = paddle_right_x - (g->paddle_w * 0.5f) - g->ball_size - 0.01f;
            reflect_on_paddle(g, 0, rel);
        }
    }

    /* 6) Scoring (ball left the field on left/right) */
    if (g->ball_x + g->ball_size < 0.0f) {
        /* point for right player */
        g->score_right++;
        game_reset_round(g, -1);
        return;
    }
    if (g->ball_x - g->ball_size > g->field_w) {
        /* point for left player */
        g->score_left++;
        game_reset_round(g, +1);
        return;
    }
}
