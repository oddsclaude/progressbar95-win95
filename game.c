/*
 * game.c - ProgressBar95 game logic (platform-independent)
 */
#include "game.h"
#include <stdlib.h>
#include <stdio.h>
#include <time.h>

int   g_barX, g_barY;
Ghost g_ghosts[MAX_GHOSTS];
int   g_gHead, g_gCount;
int   g_progress;
int   g_perfectionist;
int   g_lives;
Dot   g_dots[NUM_DOTS];
int   g_gameOver;
int   g_bsod;
int   g_level;
int   g_score;
int   g_animTick;
int   g_null_ctr;
int   g_neg_ctr;
int   g_bar_display_pct;
char  g_bar_label[16];

static int g_gTimer;
static int g_dragging;
static int g_dragOffX, g_dragOffY;

static DotKind rand_kind(void) {
    int r = rand() % 20;
    if (r < 5)  return DOT_BLUE;
    if (r < 8)  return DOT_ORANGE;
    if (r < 10) return DOT_PINK;
    if (r < 12) return DOT_GREY;
    if (r < 15) return DOT_RED;
    if (r < 18) return DOT_RANDOM;
    return DOT_GREEN;
}

static void spawn_one_dot(int i) {
    g_dots[i].x    = 10 + rand() % (AREA_W - 20);
    g_dots[i].y    = -(rand() % 80) * 256;  /* stagger 0-80px above screen */
    g_dots[i].vy   = 512 + rand() % 512;    /* 2-4 px/tick = 50-100 px/s at 25 ticks/s */
    g_dots[i].kind = rand_kind();
    g_dots[i].alive = 1;
}

static void spawn_dots(void) {
    int i;
    for (i = 0; i < NUM_DOTS; i++) spawn_one_dot(i);
}

static void reset_bar(void) {
    g_barX = AREA_W / 2 - BAR_W / 2;
    g_barY = AREA_H / 2;
    g_gHead = 0; g_gCount = 0; g_gTimer = 0;
    g_dragging = 0;
}

static void update_bar_display(void) {
    if (g_null_ctr > 0) {
        g_bar_display_pct = g_null_ctr;
        sprintf(g_bar_label, "NULL%d", g_null_ctr);
    } else if (g_neg_ctr > 0) {
        g_bar_display_pct = g_neg_ctr;
        sprintf(g_bar_label, "%%-%d", g_neg_ctr);
    } else {
        g_bar_display_pct = g_progress;
        g_bar_label[0] = '\0';
    }
}

static void apply_dot(DotKind k) {
    if (k == DOT_RANDOM) k = (DotKind)(rand() % 7);
    switch (k) {
        case DOT_BLUE:
            g_null_ctr = 0; g_neg_ctr = 0;
            g_progress += 5; g_score += 10;
            break;
        case DOT_ORANGE:
            g_null_ctr = 0; g_neg_ctr = 0;
            g_progress += 5; g_score += 5; g_perfectionist = 0;
            break;
        case DOT_PINK:
            g_null_ctr = 0;
            g_neg_ctr += 5;
            g_progress -= 5;
            if (g_neg_ctr >= 100) { g_neg_ctr = 100; g_progress = 100; }
            break;
        case DOT_GREY:
            g_neg_ctr = 0;
            g_null_ctr += 5;
            if (g_null_ctr >= 100) { g_null_ctr = 100; g_progress = 100; }
            break;
        case DOT_RED:
            g_null_ctr = 0; g_neg_ctr = 0;
            g_bsod = 1; g_gameOver = 1;
            break;
        case DOT_GREEN:
            g_null_ctr = 0; g_neg_ctr = 0;
            g_progress = 100; g_score += 50;
            break;
        default: break;
    }
    if (g_progress < 0)   g_progress = 0;
    if (g_progress > 100) g_progress = 100;
    update_bar_display();
}

void game_init(void) {
    srand((unsigned int)time(NULL));
    game_restart();
}

void game_restart(void) {
    g_progress = 0;
    g_perfectionist = 1;
    g_lives = 2;
    g_level = 1;
    g_score = 0;
    g_gameOver = 0;
    g_bsod = 0;
    g_animTick = 0;
    g_null_ctr = 0;
    g_neg_ctr = 0;
    g_bar_display_pct = 0;
    g_bar_label[0] = '\0';
    reset_bar();
    spawn_dots();
}

void game_dismiss_bsod(void) {
    g_bsod = 0;
    g_gameOver = 0;
    g_lives--;
    if (g_lives < 0) {
        g_gameOver = 1;
    } else {
        reset_bar();
        spawn_dots();
    }
}

void game_click(int mx, int my) {
    if (g_gameOver || g_bsod) return;
    g_dragging = 1;
    g_dragOffX = mx - g_barX;
    g_dragOffY = my - g_barY;
}

void game_drag(int mx, int my) {
    if (!g_dragging || g_gameOver) return;
    g_barX = mx - g_dragOffX;
    g_barY = my - g_dragOffY;
    if (g_barX < 0)              g_barX = 0;
    if (g_barX + BAR_W > AREA_W) g_barX = AREA_W - BAR_W;
    if (g_barY < 0)              g_barY = 0;
    if (g_barY + BAR_H > AREA_H) g_barY = AREA_H - BAR_H;
}

void game_release(void) {
    g_dragging = 0;
}

void game_tick(void) {
    int i;

    g_animTick++;

    /* Ghost trail */
    g_gTimer++;
    if (g_gTimer >= 2) {
        g_gTimer = 0;
        g_ghosts[g_gHead].x = g_barX;
        g_ghosts[g_gHead].y = g_barY;
        g_gHead = (g_gHead + 1) % MAX_GHOSTS;
        if (g_gCount < MAX_GHOSTS) g_gCount++;
    }

    /* Move dots down + collision check */
    for (i = 0; i < NUM_DOTS; i++) {
        int px, py, bx, by, bx2, by2, dx2, dy2;
        if (!g_dots[i].alive) continue;

        g_dots[i].y += g_dots[i].vy;
        px = g_dots[i].x;
        py = g_dots[i].y / 256;

        /* Fell off bottom - respawn at top */
        if (py > AREA_H + 10) {
            spawn_one_dot(i);
            continue;
        }

        /* Skip collision until on screen */
        if (py < -16) continue;

        bx = g_barX; by = g_barY;
        bx2 = g_barX + BAR_W; by2 = g_barY + BAR_H;
        dx2 = px + 12; dy2 = py + 16;
        if (bx < dx2 && bx2 > px && by < dy2 && by2 > py) {
            g_dots[i].alive = 0;
            apply_dot(g_dots[i].kind);
            if (g_gameOver) return;
            spawn_one_dot(i);
        }
    }

    /* Level complete */
    if (g_progress >= 100) {
        g_level++;
        g_score += 100 + (g_perfectionist ? 50 : 0);
        g_progress = 0;
        g_perfectionist = 1;
        g_null_ctr = 0;
        g_neg_ctr = 0;
        g_bar_display_pct = 0;
        g_bar_label[0] = '\0';
        reset_bar();
        spawn_dots();
        return;
    }
}
