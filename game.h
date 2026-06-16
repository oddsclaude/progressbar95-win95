/*
 * game.h - ProgressBar95 shared game state and interface
 */
#ifndef GAME_H
#define GAME_H

#define AREA_W   640
#define AREA_H   460
#define BAR_W    130
#define BAR_H    22
#define MAX_GHOSTS 80
#define NUM_DOTS   16

typedef enum {
    DOT_BLUE, DOT_ORANGE, DOT_PINK, DOT_GREY,
    DOT_RED, DOT_RANDOM, DOT_GREEN
} DotKind;

typedef struct { int x, y; } Ghost;

typedef struct {
    int x, y;
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

/* Game API */
void game_init(void);
void game_tick(void);             /* call every ~40ms when not game over */
void game_drag(int mx, int my);   /* mouse drag - pass screen coords */
void game_click(int mx, int my);  /* mouse down */
void game_release(void);          /* mouse up */
void game_restart(void);          /* restart after game over */
void game_dismiss_bsod(void);     /* press any key on BSOD screen */

#endif
