/*
 * game.c - ProgressBar95 game logic (platform-independent)
 */
#include "game.h"
#include <stdlib.h>
#include <stdio.h>
#include <time.h>

int      g_barX, g_barY;
Ghost    g_ghosts[MAX_GHOSTS];
int      g_gHead, g_gCount;
int      g_ghost_age;
int      g_progress;
int      g_perfectionist;
int      g_lives;
Seg      g_segs[NUM_SEGS];
int      g_gameOver;
int      g_bsod;
int      g_level;
int      g_score;
int      g_animTick;
SegKind  g_bar_segs[MAX_SEGS];
int      g_bar_seg_count;
char     g_bar_label[16];
int      g_null_active;
int      g_pink_active;
int      g_random_active;
Particle g_particles[MAX_PARTICLES];
int      g_particle_count;

static int g_gTimer;
static int g_dragging;
static int g_dragOffX, g_dragOffY;

/* --- bar segment helpers --- */

static int seg_count(SegKind k) {
    int i, n = 0;
    for (i = 0; i < g_bar_seg_count; i++)
        if (g_bar_segs[i] == k) n++;
    return n;
}

static int bar_has(SegKind k) {
    int i;
    for (i = 0; i < g_bar_seg_count; i++)
        if (g_bar_segs[i] == k) return 1;
    return 0;
}

static int bar_all(SegKind k) {
    int i;
    if (!g_bar_seg_count) return 0;
    for (i = 0; i < g_bar_seg_count; i++)
        if (g_bar_segs[i] != k) return 0;
    return 1;
}

static void push_seg(SegKind k) {
    if (g_bar_seg_count < MAX_SEGS)
        g_bar_segs[g_bar_seg_count++] = k;
}

static void pop_seg(void) {
    if (g_bar_seg_count > 0) g_bar_seg_count--;
}

static void clear_bar(void) {
    g_bar_seg_count = 0;
}

static void update_display(void) {
    int grey_n, pink_n;
    g_progress = seg_count(SEG_BLUE) * 5;
    grey_n = seg_count(SEG_GREY);
    pink_n = seg_count(SEG_PINK);
    if (g_null_active) {
        sprintf(g_bar_label, "NULL%d", grey_n * 5);
    } else if (g_pink_active) {
        sprintf(g_bar_label, "%%-%d", pink_n * 5);
    } else {
        g_bar_label[0] = '\0';
    }
}

/* --- falling segment helpers --- */

static SegKind rand_kind(void) {
    int r = rand() % 40;
    if (r < 20) return SEG_BLUE;
    if (r < 24) return SEG_YELLOW;
    if (r < 27) return SEG_PINK;
    if (r < 30) return SEG_GREY;
    if (r < 34) return SEG_RED;
    if (r < 36) return SEG_RANDOM;
    if (r < 37) return SEG_GREEN;  /* 1/40 = 2.5% (was 2/40) */
    return SEG_CYAN;
}

static SegKind random_resolve(void) {
    int r = rand() % 5;
    switch (r) {
        case 0: return SEG_BLUE;
        case 1: return SEG_YELLOW;
        case 2: return SEG_PINK;
        case 3: return SEG_GREY;
        default: return SEG_RED;
    }
}

static int kind_vy(SegKind k) {
    if (k == SEG_GREEN)  return 4096;  /* 2x: was 2048 */
    if (k == SEG_RANDOM) return 1024;  /* 2x: was 512 */
    return 2048;                        /* 2x: was 1024 */
}

static void init_seg(int i) {
    g_segs[i].x     = 10 + rand() % (AREA_W - 20);
    g_segs[i].y     = -(i * (AREA_H / NUM_SEGS)) * 256;
    g_segs[i].kind  = rand_kind();
    g_segs[i].vy    = kind_vy(g_segs[i].kind);
    g_segs[i].alive = 1;
    g_segs[i].value = (g_segs[i].kind == SEG_CYAN) ? (2 + rand() % 2) : 0;
}

static void respawn_seg(int i) {
    g_segs[i].x     = 10 + rand() % (AREA_W - 20);
    g_segs[i].y     = -(2 + rand() % 20) * 256;
    g_segs[i].kind  = rand_kind();
    g_segs[i].vy    = kind_vy(g_segs[i].kind);
    g_segs[i].alive = 1;
    g_segs[i].value = (g_segs[i].kind == SEG_CYAN) ? (2 + rand() % 2) : 0;
}

static void spawn_segs(void) {
    int i;
    for (i = 0; i < NUM_SEGS; i++) init_seg(i);
}

static void reset_bar_pos(void) {
    g_barX = AREA_W / 2 - BAR_W / 2;
    g_barY = AREA_H - 80;
    g_gHead = 0; g_gCount = 0; g_gTimer = 0; g_ghost_age = 0;
    g_dragging = 0;
}

static void apply_seg(SegKind k, int value) {
    int is_random;
    is_random = (k == SEG_RANDOM);
    if (is_random) {
        if (g_bar_seg_count == 0) g_random_active = 1;
        k = random_resolve();
    }
    switch (k) {
        case SEG_BLUE:
            g_null_active = 0; g_pink_active = 0;
            push_seg(SEG_BLUE);
            g_score += 10;
            break;
        case SEG_YELLOW:
            g_null_active = 0; g_pink_active = 0;
            push_seg(SEG_YELLOW);
            g_perfectionist = 0;
            break;
        case SEG_PINK:
            g_null_active = 0;
            if (g_bar_seg_count == 0 || bar_all(SEG_PINK)) {
                g_pink_active = 1;
                push_seg(SEG_PINK);
            } else {
                pop_seg();
                if (g_bar_seg_count == 0 || bar_all(SEG_PINK))
                    g_pink_active = 1;
            }
            break;
        case SEG_GREY:
            g_pink_active = 0;
            if (g_bar_seg_count == 0 || bar_has(SEG_GREY)) {
                if (g_bar_seg_count == 0) g_null_active = 1;
                push_seg(SEG_GREY);
            }
            break;
        case SEG_RED:
            g_bsod = 1; g_gameOver = 1;
            break;
        case SEG_GREEN: {
            int i;
            clear_bar();
            for (i = 0; i < 20; i++) push_seg(SEG_BLUE);
            g_null_active = 0; g_pink_active = 0;
            g_score += 50;
            break;
        }
        case SEG_CYAN: {
            int n, i;
            n = (value >= 2) ? value : (2 + rand() % 2);
            g_null_active = 0; g_pink_active = 0;
            for (i = 0; i < n; i++) push_seg(SEG_BLUE);
            g_score += 15 * n;
            break;
        }
        default: break;
    }
    update_display();
}

/* --- particle helpers --- */

static void seg_rgb(SegKind k, unsigned char *cr, unsigned char *cg, unsigned char *cb) {
    switch (k) {
        case SEG_BLUE:   *cr=0;   *cg=0;   *cb=170; return;
        case SEG_YELLOW: *cr=255; *cg=140; *cb=0;   return;
        case SEG_PINK:   *cr=255; *cg=0;   *cb=200; return;
        case SEG_GREY:   *cr=170; *cg=170; *cb=170; return;
        case SEG_GREEN:  *cr=0;   *cg=200; *cb=0;   return;
        case SEG_CYAN:   *cr=0;   *cg=190; *cb=255; return;
        default:         *cr=255; *cg=255; *cb=255; return;
    }
}

static void spawn_particles(int x, int y, SegKind k) {
    int i;
    unsigned char cr, cg, cb;
    seg_rgb(k, &cr, &cg, &cb);
    for (i = 0; i < 5 && g_particle_count < MAX_PARTICLES; i++) {
        Particle *p = &g_particles[g_particle_count++];
        p->x  = x + 5;
        p->y  = y;
        p->vx = (rand() % 5) - 2;
        p->vy = -(rand() % 4) - 1;
        p->life = 8 + rand() % 8;
        p->cr = cr; p->cg = cg; p->cb = cb;
    }
}

void game_init(void) {
    srand((unsigned int)time(NULL));
    game_restart();
}

void game_restart(void) {
    clear_bar();
    g_progress = 0;
    g_perfectionist = 1;
    g_lives = 2;
    g_level = 1;
    g_score = 0;
    g_gameOver = 0;
    g_bsod = 0;
    g_animTick = 0;
    g_null_active = 0;
    g_pink_active = 0;
    g_random_active = 0;
    g_bar_label[0] = '\0';
    g_particle_count = 0;
    reset_bar_pos();
    spawn_segs();
}

void game_dismiss_bsod(void) {
    g_bsod = 0;
    g_gameOver = 0;
    g_lives--;
    g_particle_count = 0;
    if (g_lives < 0) {
        g_gameOver = 1;
    } else {
        reset_bar_pos();
        spawn_segs();
    }
}

void game_click(int mx, int my) {
    if (g_gameOver || g_bsod) return;
    g_dragging = 1;
    g_dragOffX = mx - g_barX;
    g_dragOffY = my - g_barY;
}

void game_drag(int mx, int my) {
    int newX, newY;
    if (!g_dragging || g_gameOver) return;
    newX = mx - g_dragOffX;
    newY = my - g_dragOffY;
    /* allow bar to slide one full width off-screen on x for edge catches */
    if (newX < -BAR_W) newX = -BAR_W;
    if (newX > AREA_W) newX = AREA_W;
    if (newY < 0) newY = 0;
    if (newY + BAR_H > AREA_H) newY = AREA_H - BAR_H;
    if (newX != g_barX || newY != g_barY) {
        g_barX = newX; g_barY = newY;
        g_ghost_age = 0;
    }
}

void game_release(void) {
    g_dragging = 0;
}

void game_tick(void) {
    int i, blue_n, grey_n, pink_n;

    g_animTick++;

    /* Ghost trail: accumulate, then instant-clear after inactivity */
    g_gTimer++;
    if (g_gTimer >= 2) {
        g_gTimer = 0;
        g_ghosts[g_gHead].x = g_barX;
        g_ghosts[g_gHead].y = g_barY;
        g_gHead = (g_gHead + 1) % MAX_GHOSTS;
        if (g_gCount < MAX_GHOSTS) g_gCount++;
    }
    g_ghost_age++;
    if (g_ghost_age > GHOST_CLEAR_AFTER && g_gCount > 0) {
        g_gHead = 0; g_gCount = 0; g_ghost_age = 0;
    }

    /* Move falling segments + collision check */
    for (i = 0; i < NUM_SEGS; i++) {
        int px, py, bx, by, bx2, by2, dx2, dy2;
        if (!g_segs[i].alive) continue;

        g_segs[i].y += g_segs[i].vy;
        px = g_segs[i].x;
        py = g_segs[i].y / 256;

        if (py > AREA_H + 10) { respawn_seg(i); continue; }
        if (py < -16) continue;

        bx  = g_barX;         by  = g_barY;
        bx2 = g_barX + BAR_W; by2 = g_barY + BAR_H;
        dx2 = px + 10;        dy2 = py + 16;

        if (bx < dx2 && bx2 > px && by < dy2 && by2 > py) {
            int catch_it, is_red_tip, seg_slot_w, filled_px;
            catch_it   = 1;
            is_red_tip = (g_segs[i].kind == SEG_RED && px < g_barX + 4);

            if (!g_null_active && !is_red_tip) {
                /* only the empty (unfilled) portion catches */
                seg_slot_w = (BAR_W - 4) / MAX_SEGS;
                filled_px  = g_barX + 2 + g_bar_seg_count * seg_slot_w;
                if (px + 5 < filled_px) catch_it = 0;
            }

            if (is_red_tip) {
                g_score += 5;
                respawn_seg(i);
                continue;
            }
            if (catch_it) {
                apply_seg(g_segs[i].kind, g_segs[i].value);
                if (g_gameOver) return;
                respawn_seg(i);
            } else {
                /* segment hit the filled portion - destroy with particles */
                spawn_particles(px, py, g_segs[i].kind);
                respawn_seg(i);
            }
        }
    }

    /* Update particles */
    for (i = 0; i < g_particle_count; ) {
        g_particles[i].x += g_particles[i].vx;
        g_particles[i].y += g_particles[i].vy;
        g_particles[i].vy++;
        if (--g_particles[i].life <= 0)
            g_particles[i] = g_particles[--g_particle_count];
        else
            i++;
    }

    /* Level complete */
    blue_n = seg_count(SEG_BLUE);
    grey_n = seg_count(SEG_GREY);
    pink_n = seg_count(SEG_PINK);

    if (blue_n >= 20 || (g_null_active && grey_n >= 20) || (g_pink_active && pink_n >= 20)) {
        int cyan_bonus;
        cyan_bonus = (blue_n >= 22) ? 200 : 0;
        g_level++;
        g_score += 100 + (g_perfectionist ? 50 : 0) + cyan_bonus;
        clear_bar();
        g_progress = 0;
        g_perfectionist = 1;
        g_null_active = 0;
        g_pink_active = 0;
        g_random_active = 0;
        g_bar_label[0] = '\0';
        g_particle_count = 0;
        reset_bar_pos();
        spawn_segs();
    }
}
