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
int   g_null_active;
int   g_pink_active;
int   g_random_active;

static int g_gTimer;
static int g_dragging;
static int g_dragOffX, g_dragOffY;

static DotKind rand_kind(void) {
    int r = rand() % 40;
    if (r < 20) return DOT_BLUE;     /* 50% */
    if (r < 24) return DOT_YELLOW;   /* 10% */
    if (r < 27) return DOT_PINK;     /*  7.5% */
    if (r < 30) return DOT_GREY;     /*  7.5% */
    if (r < 34) return DOT_RED;      /* 10% */
    if (r < 36) return DOT_RANDOM;   /*  5% */
    if (r < 38) return DOT_GREEN;    /*  5% */
    return DOT_CYAN;                 /*  5% */
}

/* random excludes green and cyan per wiki */
static DotKind random_resolve(void) {
    int r = rand() % 5;
    switch (r) {
        case 0: return DOT_BLUE;
        case 1: return DOT_YELLOW;
        case 2: return DOT_PINK;
        case 3: return DOT_GREY;
        default: return DOT_RED;
    }
}

static int kind_vy(DotKind k) {
    if (k == DOT_GREEN)  return 2048; /* very fast */
    if (k == DOT_RANDOM) return 512;  /* slow, constant */
    return 1024;                      /* normal */
}

static void init_dot(int i) {
    g_dots[i].x     = 10 + rand() % (AREA_W - 20);
    g_dots[i].y     = -(i * (AREA_H / NUM_DOTS)) * 256; /* even stagger above screen */
    g_dots[i].kind  = rand_kind();
    g_dots[i].vy    = kind_vy(g_dots[i].kind);
    g_dots[i].alive = 1;
}

static void respawn_dot(int i) {
    g_dots[i].x     = 10 + rand() % (AREA_W - 20);
    g_dots[i].y     = -(2 + rand() % 20) * 256;
    g_dots[i].kind  = rand_kind();
    g_dots[i].vy    = kind_vy(g_dots[i].kind);
    g_dots[i].alive = 1;
}

static void spawn_dots(void) {
    int i;
    for (i = 0; i < NUM_DOTS; i++) init_dot(i);
}

static void reset_bar(void) {
    g_barX = AREA_W / 2 - BAR_W / 2;
    g_barY = AREA_H - 80;
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
    int is_random;
    is_random = (k == DOT_RANDOM);
    if (is_random) {
        if (g_progress == 0) g_random_active = 1;
        k = random_resolve();
    }
    switch (k) {
        case DOT_BLUE:
            g_null_ctr = 0; g_neg_ctr = 0;
            g_null_active = 0; g_pink_active = 0;
            g_progress += 5; g_score += 10;
            break;
        case DOT_YELLOW:
            g_null_ctr = 0; g_neg_ctr = 0;
            g_null_active = 0; g_pink_active = 0;
            g_perfectionist = 0;
            /* no progress, no score */
            break;
        case DOT_PINK:
            g_null_ctr = 0; g_null_active = 0;
            if (g_progress == 0 && !g_pink_active) g_pink_active = 1;
            g_neg_ctr += 5;
            g_progress -= 5;
            if (g_neg_ctr >= 100) { g_neg_ctr = 100; g_progress = 100; }
            break;
        case DOT_GREY:
            g_neg_ctr = 0; g_pink_active = 0;
            if (g_progress == 0 && !g_null_active) g_null_active = 1;
            g_null_ctr += 5;
            if (g_null_ctr >= 100) { g_null_ctr = 100; g_progress = 100; }
            break;
        case DOT_RED:
            g_null_ctr = 0; g_neg_ctr = 0;
            g_null_active = 0; g_pink_active = 0;
            g_bsod = 1; g_gameOver = 1;
            break;
        case DOT_GREEN:
            g_null_ctr = 0; g_neg_ctr = 0;
            g_null_active = 0; g_pink_active = 0;
            g_progress = 100; g_score += 50;
            break;
        case DOT_CYAN: {
            int mult;
            mult = (rand() % 2) ? 3 : 2;
            g_null_ctr = 0; g_neg_ctr = 0;
            g_null_active = 0; g_pink_active = 0;
            g_progress += 5 * mult;
            g_score += 15 * mult;
            break;
        }
        default: break;
    }
    if (g_progress < 0)   g_progress = 0;
    if (g_progress > 110) g_progress = 110;
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
    g_null_active = 0;
    g_pink_active = 0;
    g_random_active = 0;
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

    /* Move dots, collision check */
    for (i = 0; i < NUM_DOTS; i++) {
        int px, py, bx, by, bx2, by2, dx2, dy2;
        if (!g_dots[i].alive) continue;

        g_dots[i].y += g_dots[i].vy;
        px = g_dots[i].x;
        py = g_dots[i].y / 256;

        if (py > AREA_H + 10) { respawn_dot(i); continue; }
        if (py < -16) continue;

        bx  = g_barX;         by  = g_barY;
        bx2 = g_barX + BAR_W; by2 = g_barY + BAR_H;
        dx2 = px + 10;        dy2 = py + 16;

        if (bx < dx2 && bx2 > px && by < dy2 && by2 > py) {
            int catch_it, fillW, fill_end, is_red_tip;
            catch_it   = 1;
            is_red_tip = (g_dots[i].kind == DOT_RED && px < g_barX + 4);

            if (!g_null_active && !is_red_tip) {
                fillW    = g_bar_display_pct * (BAR_W - 4) / 100;
                if (fillW > BAR_W - 4) fillW = BAR_W - 4;
                fill_end = g_barX + 2 + fillW;
                if (px + 5 < fill_end) catch_it = 0;
            }

            if (is_red_tip) {
                /* skill shot: crash red with left tip, safe */
                g_score += 5;
                respawn_dot(i);
                continue;
            }
            if (catch_it) {
                apply_dot(g_dots[i].kind);
                if (g_gameOver) return;
                respawn_dot(i);
            }
        }
    }

    /* Level complete */
    if (g_progress >= 100) {
        int cyan_bonus;
        cyan_bonus = (g_progress >= 110) ? 200 : 0;
        g_level++;
        g_score += 100 + (g_perfectionist ? 50 : 0) + cyan_bonus;
        g_progress = 0;
        g_perfectionist = 1;
        g_null_ctr = 0;
        g_neg_ctr = 0;
        g_null_active = 0;
        g_pink_active = 0;
        g_random_active = 0;
        g_bar_display_pct = 0;
        g_bar_label[0] = '\0';
        reset_bar();
        spawn_dots();
        return;
    }
}
