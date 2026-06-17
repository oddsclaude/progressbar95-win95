/*
 * game.h - ProgressBar95 shared state and API
 */
#ifndef GAME_H
#define GAME_H

#define AREA_W  640
#define AREA_H  460
#define BAR_W   180   /* 45% of original 400 */
#define BAR_H    24
#define MAX_SEGS 22   /* 20 = 100%, 22 = 110% via cyan */
#define NUM_SEGS  4   /* falling segments (2-4 visible at once) */
#define MAX_GHOSTS 80
#define GHOST_CLEAR_AFTER 8  /* ticks of no bar movement -> ghosts vanish */

typedef enum {
    SEG_BLUE,    /* +5% (1 slot), most common */
    SEG_YELLOW,  /* corrupted: occupies slot, no progress, ruins perfectionist */
    SEG_PINK,    /* removes last seg; adds SEG_PINK only when bar empty/all-pink */
    SEG_GREY,    /* adds only when bar empty or already has grey (NULL mode) */
    SEG_RED,     /* BSOD; left-tip crash = safe */
    SEG_RANDOM,  /* slow, flashing; resolves to blue/yellow/pink/grey/red */
    SEG_GREEN,   /* fills bar instantly (20 blue), very fast */
    SEG_CYAN     /* pushes 2 or 3 SEG_BLUE; can reach 110% */
} SegKind;

typedef struct { int x, y; } Ghost;

typedef struct {
    int x;
    int y;        /* fixed-point: actual pixels = y / 256 */
    int vy;       /* fall speed, fixed-point per tick */
    SegKind kind;
    int alive;
} Seg;

extern int     g_barX, g_barY;
extern Ghost   g_ghosts[MAX_GHOSTS];
extern int     g_gHead, g_gCount;
extern int     g_ghost_age;     /* ticks since last bar move */
extern int     g_progress;      /* blue_count * 5 */
extern int     g_perfectionist;
extern int     g_lives;
extern Seg     g_segs[NUM_SEGS];
extern int     g_gameOver;
extern int     g_bsod;
extern int     g_level;
extern int     g_score;
extern int     g_animTick;
extern SegKind g_bar_segs[MAX_SEGS];
extern int     g_bar_seg_count;
extern char    g_bar_label[16]; /* "NULL##" / "%%-%d" / empty */
extern int     g_null_active;   /* NULL mode: transparent fill, any part catches */
extern int     g_pink_active;   /* magic pink mode */
extern int     g_random_active; /* ??? mode triggered by random at empty bar */

void game_init(void);
void game_tick(void);
void game_drag(int mx, int my);
void game_click(int mx, int my);
void game_release(void);
void game_restart(void);
void game_dismiss_bsod(void);

#endif
