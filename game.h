/*
 * game.h - ProgressBar95 shared game state and interface
 */
#ifndef GAME_H
#define GAME_H

#define AREA_W   640
#define AREA_H   460
#define BAR_W    400
#define BAR_H    24
#define MAX_GHOSTS 80
#define NUM_DOTS   16

typedef enum {
    DOT_BLUE,    /* +5%, most common */
    DOT_YELLOW,  /* corrupted: no progress/points, ruins perfectionist */
    DOT_PINK,    /* -5% progress */
    DOT_GREY,    /* neutral, NULL mode counter */
    DOT_RED,     /* BSOD (left-tip crash is safe) */
    DOT_RANDOM,  /* slow, resolves to blue/yellow/pink/grey/red */
    DOT_GREEN,   /* instant 100%, very fast */
    DOT_CYAN     /* +10% or +15%, can push to 110% */
} DotKind;

typedef struct { int x, y; } Ghost;

typedef struct {
    int x;
    int y;       /* fixed-point: actual pixels = y / 256 */
    int vy;      /* fall speed, fixed-point per tick */
    DotKind kind;
    int alive;
} Dot;

/* Game state - read by platform renderers */
extern int   g_barX, g_barY;
extern Ghost g_ghosts[MAX_GHOSTS];
extern int   g_gHead, g_gCount;
extern int   g_progress;
extern int   g_perfectionist;
extern int   g_lives;
extern Dot   g_dots[NUM_DOTS];
extern int   g_gameOver;
extern int   g_bsod;
extern int   g_level;
extern int   g_score;
extern int   g_animTick;
extern int   g_null_ctr;        /* grey-only hidden fill counter (0-100) */
extern int   g_neg_ctr;         /* pink-only hidden fill counter (0-100) */
extern int   g_bar_display_pct; /* what the bar fill actually shows */
extern char  g_bar_label[16];   /* bar label override; empty = normal N% */
extern int   g_null_active;     /* NULL mode: fill transparent, any part catches */
extern int   g_pink_active;     /* Magic Pink mode: fill renders pink */
extern int   g_random_active;   /* ??? mode: triggered by random at 0% */

/* Game API */
void game_init(void);
void game_tick(void);
void game_drag(int mx, int my);
void game_click(int mx, int my);
void game_release(void);
void game_restart(void);
void game_dismiss_bsod(void);

#endif
