/*
 * ProgressBar95 - SDL2 cross-platform version
 * Linux/Wayland: gcc -o progressbar95 progressbar95-sdl2.c -lSDL2 -lSDL2_ttf
 * Windows (Win98+): same command with MinGW
 * macOS: same command
 *
 * Run on Wayland: SDL_VIDEODRIVER=wayland ./progressbar95
 *
 * Game logic is identical to the Win32 version.
 * Needs: libsdl2-dev libsdl2-ttf-dev (apt) / sdl2 sdl2_ttf (pacman)
 */

#include <SDL2/SDL.h>
#include <SDL2/SDL_ttf.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <stdio.h>

#define AREA_W   640
#define AREA_H   460
#define WIN_W    AREA_W
#define WIN_H    (AREA_H + 22)

#define BAR_W    130
#define BAR_H    22

#define MAX_GHOSTS 80
#define NUM_DOTS   16

#define TICK_MS  40

typedef enum { DOT_BLUE, DOT_ORANGE, DOT_PINK, DOT_GREY, DOT_RED, DOT_RANDOM, DOT_GREEN } DotKind;

typedef struct { int x, y; } Ghost;

typedef struct {
    int x, y;
    DotKind kind;
    int alive;
} Dot;

static int    g_barX, g_barY, g_dx, g_dy;
static Ghost  g_ghosts[MAX_GHOSTS];
static int    g_gHead, g_gCount, g_gTimer;
static int    g_progress;
static int    g_perfectionist;
static int    g_lives;
static Dot    g_dots[NUM_DOTS];
static int    g_gameOver;
static int    g_bsod;
static int    g_level;
static int    g_score;
static int    g_animTick;

static SDL_Window   *g_win;
static SDL_Renderer *g_ren;
static TTF_Font     *g_font;
static TTF_Font     *g_fontLarge;

typedef struct { Uint8 r, g, b; } Col;
static Col DotCol(DotKind k) {
    switch (k) {
        case DOT_BLUE:   return (Col){0,   80,  220};
        case DOT_ORANGE: return (Col){255, 140, 0  };
        case DOT_PINK:   return (Col){255, 50,  150};
        case DOT_GREY:   return (Col){150, 150, 150};
        case DOT_RED:    return (Col){200, 0,   0  };
        case DOT_RANDOM: return (Col){180, 0,   200};
        case DOT_GREEN:  return (Col){0,   180, 0  };
        default:         return (Col){255, 255, 255};
    }
}

static Col CycleCol(int tick) {
    static Col cycle[] = {
        {0,80,220},{255,140,0},{255,50,150},{150,150,150},{200,0,0},{0,180,0}
    };
    return cycle[(tick / 6) % 6];
}

static DotKind RandKind(void) {
    int r = rand() % 20;
    if (r < 5)  return DOT_BLUE;
    if (r < 8)  return DOT_ORANGE;
    if (r < 10) return DOT_PINK;
    if (r < 12) return DOT_GREY;
    if (r < 15) return DOT_RED;
    if (r < 18) return DOT_RANDOM;
    return DOT_GREEN;
}

static void ResetBar(void) {
    g_barX = AREA_W / 2 - BAR_W / 2;
    g_barY = AREA_H / 2;
    g_dx = 3; g_dy = 0;
    g_gHead = 0; g_gCount = 0; g_gTimer = 0;
}

static void SpawnDots(void) {
    int i;
    for (i = 0; i < NUM_DOTS; i++) {
        g_dots[i].x = 20 + rand() % (AREA_W - 50);
        g_dots[i].y = 20 + rand() % (AREA_H - 50);
        g_dots[i].kind = RandKind();
        g_dots[i].alive = 1;
    }
}

static void StartGame(void) {
    g_progress = 0;
    g_perfectionist = 1;
    g_lives = 2;
    g_level = 1;
    g_score = 0;
    g_gameOver = 0;
    g_bsod = 0;
    ResetBar();
    SpawnDots();
}

static void ApplyDot(DotKind k) {
    if (k == DOT_RANDOM) k = (DotKind)(rand() % 7);
    switch (k) {
        case DOT_BLUE:   g_progress += 5; g_score += 10; break;
        case DOT_ORANGE: g_progress += 5; g_score += 5; g_perfectionist = 0; break;
        case DOT_PINK:   g_progress -= 5; g_perfectionist = 0; break;
        case DOT_GREY:   break;
        case DOT_RED:    g_bsod = 1; g_gameOver = 1; break;
        case DOT_GREEN:  g_progress = 100; g_score += 50; break;
        default: break;
    }
    if (g_progress < 0)   g_progress = 0;
    if (g_progress > 100) g_progress = 100;
}

static void GameTick(void) {
    int i;
    SDL_Rect barR, dotR;

    g_barX += g_dx;
    g_barY += g_dy;
    if (g_barX < 0)              { g_barX = 0;              g_dx = -g_dx; }
    if (g_barX + BAR_W > AREA_W) { g_barX = AREA_W - BAR_W; g_dx = -g_dx; }
    if (g_barY < 0)              { g_barY = 0;              g_dy = -g_dy; }
    if (g_barY + BAR_H > AREA_H) { g_barY = AREA_H - BAR_H; g_dy = -g_dy; }

    g_animTick++;

    g_gTimer++;
    if (g_gTimer >= 2) {
        g_gTimer = 0;
        g_ghosts[g_gHead].x = g_barX;
        g_ghosts[g_gHead].y = g_barY;
        g_gHead = (g_gHead + 1) % MAX_GHOSTS;
        if (g_gCount < MAX_GHOSTS) g_gCount++;
    }

    barR.x = g_barX; barR.y = g_barY; barR.w = BAR_W; barR.h = BAR_H;
    for (i = 0; i < NUM_DOTS; i++) {
        if (!g_dots[i].alive) continue;
        dotR.x = g_dots[i].x; dotR.y = g_dots[i].y; dotR.w = 16; dotR.h = 16;
        if (SDL_HasIntersection(&barR, &dotR)) {
            g_dots[i].alive = 0;
            ApplyDot(g_dots[i].kind);
            if (g_gameOver) return;
        }
    }

    if (g_progress >= 100) {
        g_level++;
        g_score += 100 + (g_perfectionist ? 50 : 0);
        g_progress = 0;
        g_perfectionist = 1;
        ResetBar();
        SpawnDots();
        return;
    }

    for (i = 0; i < NUM_DOTS; i++) {
        if (!g_dots[i].alive && rand() % 300 == 0) {
            g_dots[i].x = 20 + rand() % (AREA_W - 50);
            g_dots[i].y = 20 + rand() % (AREA_H - 50);
            g_dots[i].kind = RandKind();
            g_dots[i].alive = 1;
        }
    }
}

static void RenderText(int x, int y, const char *text, Uint8 r, Uint8 g, Uint8 b) {
    SDL_Color col = {r, g, b, 255};
    SDL_Surface *surf;
    SDL_Texture *tex;
    SDL_Rect dst;
    if (!g_font || !text || !text[0]) return;
    surf = TTF_RenderText_Solid(g_font, text, col);
    if (!surf) return;
    tex = SDL_CreateTextureFromSurface(g_ren, surf);
    dst.x = x; dst.y = y; dst.w = surf->w; dst.h = surf->h;
    SDL_RenderCopy(g_ren, tex, NULL, &dst);
    SDL_DestroyTexture(tex);
    SDL_FreeSurface(surf);
}

static void RenderTextLarge(int x, int y, const char *text, Uint8 r, Uint8 g, Uint8 b) {
    SDL_Color col = {r, g, b, 255};
    SDL_Surface *surf;
    SDL_Texture *tex;
    SDL_Rect dst;
    TTF_Font *f = g_fontLarge ? g_fontLarge : g_font;
    if (!f || !text || !text[0]) return;
    surf = TTF_RenderText_Solid(f, text, col);
    if (!surf) return;
    tex = SDL_CreateTextureFromSurface(g_ren, surf);
    dst.x = x; dst.y = y; dst.w = surf->w; dst.h = surf->h;
    SDL_RenderCopy(g_ren, tex, NULL, &dst);
    SDL_DestroyTexture(tex);
    SDL_FreeSurface(surf);
}

static void DrawBar(int x, int y, int prog) {
    SDL_Rect r;
    int fillW = prog * (BAR_W - 4) / 100;
    char pct[8];

    SDL_SetRenderDrawColor(g_ren, 192, 192, 192, 255);
    r.x = x; r.y = y; r.w = BAR_W; r.h = BAR_H;
    SDL_RenderFillRect(g_ren, &r);
    SDL_SetRenderDrawColor(g_ren, 0, 0, 0, 255);
    SDL_RenderDrawRect(g_ren, &r);

    if (fillW > 0) {
        SDL_SetRenderDrawColor(g_ren, 0, 0, 128, 255);
        r.x = x+2; r.y = y+2; r.w = fillW; r.h = BAR_H-4;
        SDL_RenderFillRect(g_ren, &r);
    }

    sprintf(pct, "%d%%", prog);
    if (fillW > BAR_W/2)
        RenderText(x + BAR_W/2 - 12, y + 3, pct, 255, 255, 255);
    else
        RenderText(x + BAR_W/2 - 12, y + 3, pct, 0, 0, 0);
}

static void DrawGhost(int x, int y, int idx, int total) {
    SDL_Rect r;
    int fade = 200 - (idx * 200 / (total > 1 ? total : 1));
    if (fade < 20) fade = 20;
    SDL_SetRenderDrawColor(g_ren, (Uint8)fade, (Uint8)fade, (Uint8)fade, 255);
    r.x = x; r.y = y; r.w = BAR_W; r.h = BAR_H;
    SDL_RenderFillRect(g_ren, &r);
}

static void DrawDot(Dot *d) {
    SDL_Rect r;
    Col c;
    const char *ch = NULL;

    if (d->kind == DOT_RED) {
        int flash = (g_animTick / 4) % 2;
        c.r = flash ? 200 : 120; c.g = 0; c.b = 0;
        ch = "!";
    } else if (d->kind == DOT_RANDOM) {
        c = CycleCol(g_animTick);
        ch = "?";
    } else {
        c = DotCol(d->kind);
        if (d->kind == DOT_GREY) ch = "0";
    }

    SDL_SetRenderDrawColor(g_ren, c.r, c.g, c.b, 255);
    r.x = d->x; r.y = d->y; r.w = 16; r.h = 16;
    SDL_RenderFillRect(g_ren, &r);
    SDL_SetRenderDrawColor(g_ren, 0, 0, 0, 255);
    SDL_RenderDrawRect(g_ren, &r);

    if (ch) {
        Uint8 tr = (d->kind == DOT_GREY) ? 50 : 255;
        RenderText(d->x + 3, d->y + 1, ch, tr, tr, tr);
    }
}

static void DrawBSOD(void) {
    SDL_Rect r;
    r.x = 0; r.y = 0; r.w = WIN_W; r.h = WIN_H;
    SDL_SetRenderDrawColor(g_ren, 0, 0, 170, 255);
    SDL_RenderFillRect(g_ren, &r);
    RenderText(50, 80,  "A fatal exception 0E has occurred at 0028:C000034F in", 255,255,255);
    RenderText(50, 98,  "VXD VMM(01) + 00010E36. The current application will be", 255,255,255);
    RenderText(50, 116, "terminated.", 255,255,255);
    RenderText(50, 152, "*  Press any key to terminate the current application.", 255,255,255);
    RenderText(50, 170, "*  Press CTRL+ALT+DEL again to restart your computer.", 255,255,255);
    RenderText(50, 188, "   You will lose any unsaved information in all", 255,255,255);
    RenderText(50, 206, "   applications.", 255,255,255);
    RenderText(50, 250, "Press any key to continue _", 255,255,255);
}

static void DrawGameOver(void) {
    char msg[128];
    SDL_Rect r;
    r.x = 0; r.y = 0; r.w = WIN_W; r.h = WIN_H;
    SDL_SetRenderDrawColor(g_ren, 0, 0, 0, 255);
    SDL_RenderFillRect(g_ren, &r);
    RenderTextLarge(WIN_W/2 - 50, WIN_H/2 - 30, "GAME OVER", 255,255,255);
    sprintf(msg, "Score: %d   Level: %d", g_score, g_level);
    RenderText(WIN_W/2 - 70, WIN_H/2 + 10, msg, 255,255,255);
    RenderText(WIN_W/2 - 90, WIN_H/2 + 40, "Press ENTER to play again", 255,255,255);
}

static void Render(void) {
    int i;
    char hud[256];
    SDL_Rect r;

    SDL_RenderClear(g_ren);

    if (g_bsod)     { DrawBSOD();     SDL_RenderPresent(g_ren); return; }
    if (g_gameOver) { DrawGameOver(); SDL_RenderPresent(g_ren); return; }

    SDL_SetRenderDrawColor(g_ren, 0, 128, 128, 255);
    r.x = 0; r.y = 0; r.w = WIN_W; r.h = AREA_H;
    SDL_RenderFillRect(g_ren, &r);

    for (i = g_gCount - 1; i >= 0; i--) {
        int idx = (g_gHead - 1 - i + MAX_GHOSTS) % MAX_GHOSTS;
        DrawGhost(g_ghosts[idx].x, g_ghosts[idx].y, i, g_gCount);
    }

    for (i = 0; i < NUM_DOTS; i++)
        if (g_dots[i].alive) DrawDot(&g_dots[i]);

    DrawBar(g_barX, g_barY, g_progress);

    SDL_SetRenderDrawColor(g_ren, 192, 192, 192, 255);
    r.x = 0; r.y = AREA_H; r.w = WIN_W; r.h = 22;
    SDL_RenderFillRect(g_ren, &r);

    sprintf(hud, "Level: %d   Score: %d   Lives: %d   %s",
        g_level, g_score, g_lives, g_perfectionist ? "[Perfectionist]" : "");
    RenderText(6, AREA_H + 3, hud, 0, 0, 0);

    SDL_RenderPresent(g_ren);
}

static TTF_Font *TryLoadFont(int size) {
    static const char *paths[] = {
        "/usr/share/fonts/truetype/dejavu/DejaVuSansMono.ttf",
        "/usr/share/fonts/TTF/DejaVuSansMono.ttf",
        "/usr/share/fonts/dejavu/DejaVuSansMono.ttf",
        "/usr/share/fonts/truetype/liberation/LiberationMono-Regular.ttf",
        "/System/Library/Fonts/Menlo.ttc",
        "C:\\Windows\\Fonts\\cour.ttf",
        NULL
    };
    int i;
    TTF_Font *f;
    for (i = 0; paths[i]; i++) {
        f = TTF_OpenFont(paths[i], size);
        if (f) return f;
    }
    return NULL;
}

int main(int argc, char *argv[]) {
    SDL_Event e;
    Uint32 lastTick, now;
    int running = 1;

    (void)argc; (void)argv;
    srand((unsigned int)time(NULL));

    SDL_Init(SDL_INIT_VIDEO);
    TTF_Init();

    g_win = SDL_CreateWindow("ProgressBar95",
        SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
        WIN_W, WIN_H, SDL_WINDOW_SHOWN);
    g_ren = SDL_CreateRenderer(g_win, -1,
        SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);

    g_font      = TryLoadFont(13);
    g_fontLarge = TryLoadFont(28);

    StartGame();
    lastTick = SDL_GetTicks();

    while (running) {
        while (SDL_PollEvent(&e)) {
            if (e.type == SDL_QUIT) { running = 0; break; }
            if (e.type == SDL_KEYDOWN) {
                if (g_bsod) {
                    g_bsod = 0; g_gameOver = 0;
                    g_lives--;
                    if (g_lives < 0) g_gameOver = 1;
                    else { ResetBar(); SpawnDots(); }
                } else if (g_gameOver) {
                    if (e.key.keysym.sym == SDLK_RETURN) StartGame();
                } else {
                    switch (e.key.keysym.sym) {
                        case SDLK_LEFT:  if (g_dx == 0) { g_dx = -3; g_dy = 0; } break;
                        case SDLK_RIGHT: if (g_dx == 0) { g_dx =  3; g_dy = 0; } break;
                        case SDLK_UP:    if (g_dy == 0) { g_dy = -3; g_dx = 0; } break;
                        case SDLK_DOWN:  if (g_dy == 0) { g_dy =  3; g_dx = 0; } break;
                        case SDLK_ESCAPE: running = 0; break;
                    }
                }
            }
        }

        now = SDL_GetTicks();
        if (now - lastTick >= (Uint32)TICK_MS) {
            lastTick = now;
            if (!g_gameOver) GameTick();
        }

        Render();
    }

    if (g_fontLarge) TTF_CloseFont(g_fontLarge);
    if (g_font)      TTF_CloseFont(g_font);
    SDL_DestroyRenderer(g_ren);
    SDL_DestroyWindow(g_win);
    TTF_Quit();
    SDL_Quit();
    return 0;
}
