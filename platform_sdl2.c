/*
 * platform_sdl2.c - SDL2 renderer for ProgressBar95
 *
 * Compile: gcc -o progressbar95 platform_sdl2.c game.c -lSDL2 -lSDL2_ttf
 * Wayland: SDL_VIDEODRIVER=wayland ./progressbar95
 */

#include <SDL2/SDL.h>
#include <SDL2/SDL_ttf.h>
#include <stdio.h>
#include <string.h>
#include "game.h"

#define WIN_W   AREA_W
#define WIN_H   (AREA_H + 22)
#define TICK_MS 40

static SDL_Window   *g_win;
static SDL_Renderer *g_ren;
static TTF_Font     *g_font;
static TTF_Font     *g_fontLg;

typedef struct { Uint8 r, g, b; } Col;

static Col seg_col(SegKind k) {
    switch (k) {
        case SEG_BLUE:   return (Col){  0,  0,170};
        case SEG_YELLOW: return (Col){255,140,  0};
        case SEG_PINK:   return (Col){255,  0,200};
        case SEG_GREY:   return (Col){170,170,170};
        case SEG_RED:    return (Col){220,  0,  0};
        case SEG_GREEN:  return (Col){  0,200,  0};
        case SEG_CYAN:   return (Col){  0,190,255};
        case SEG_RANDOM: return (Col){255,255,255};
        default:         return (Col){255,255,255};
    }
}

static Col cycle_col(void) {
    static Col c[] = {
        {0,0,170},{255,140,0},{255,0,200},{170,170,170},{220,0,0},{0,200,0}
    };
    return c[(g_animTick/5)%6];
}

static void rtext(TTF_Font *f, int x, int y, const char *s, Uint8 r, Uint8 g, Uint8 b) {
    SDL_Color col = {r,g,b,255};
    SDL_Surface *sur;
    SDL_Texture *tex;
    SDL_Rect dst;
    if (!f || !s || !s[0]) return;
    sur = TTF_RenderText_Solid(f, s, col);
    if (!sur) return;
    tex = SDL_CreateTextureFromSurface(g_ren, sur);
    dst.x=x; dst.y=y; dst.w=sur->w; dst.h=sur->h;
    SDL_RenderCopy(g_ren, tex, NULL, &dst);
    SDL_DestroyTexture(tex);
    SDL_FreeSurface(sur);
}

static void rtxt(int x, int y, const char *s, Uint8 r, Uint8 g, Uint8 b) {
    rtext(g_font, x, y, s, r, g, b);
}

static void rtxt_lg(int x, int y, const char *s, Uint8 r, Uint8 g, Uint8 b) {
    rtext(g_fontLg ? g_fontLg : g_font, x, y, s, r, g, b);
}

static void fill(int x, int y, int w, int h, Uint8 r, Uint8 g, Uint8 b) {
    SDL_Rect rc = {x,y,w,h};
    SDL_SetRenderDrawColor(g_ren,r,g,b,255);
    SDL_RenderFillRect(g_ren,&rc);
}

static void hline(int x, int y, int w, Uint8 r, Uint8 g, Uint8 b) { fill(x,y,w,1,r,g,b); }
static void vline(int x, int y, int h, Uint8 r, Uint8 g, Uint8 b) { fill(x,y,1,h,r,g,b); }

static void win95_sunken(int x, int y, int w, int h) {
    hline(x,     y,     w, 128,128,128);
    vline(x,     y,     h, 128,128,128);
    hline(x,     y+h-1, w, 255,255,255);
    vline(x+w-1, y,     h, 255,255,255);
    hline(x+1,   y+1,   w-2, 0,0,0);
    vline(x+1,   y+1,   h-2, 0,0,0);
    hline(x+1,   y+h-2, w-2, 192,192,192);
    vline(x+w-2, y+1,   h-2, 192,192,192);
}

static void draw_bar(int x, int y) {
    int i, seg_w, sx, on_fill;
    char pct[20];

    seg_w = (BAR_W - 4) / MAX_SEGS;

    fill(x, y, BAR_W, BAR_H, 212,208,200);

    if (!g_null_active) {
        for (i = 0; i < g_bar_seg_count; i++) {
            Col c = seg_col(g_bar_segs[i]);
            sx = x + 2 + i * seg_w;
            fill(sx, y+2, seg_w-1, BAR_H-4, c.r, c.g, c.b);
        }
    }

    win95_sunken(x, y, BAR_W, BAR_H);

    if (g_bar_label[0]) {
        strncpy(pct, g_bar_label, sizeof(pct)-1);
        pct[sizeof(pct)-1] = '\0';
    } else {
        sprintf(pct, "%d %%", g_progress);
    }
    on_fill = !g_null_active && g_bar_seg_count * seg_w > BAR_W / 2;
    rtxt(x + BAR_W/2 - 16, y + (BAR_H-13)/2, pct,
         on_fill?255:0, on_fill?255:0, on_fill?255:0);
}

static void draw_ghost(int x, int y) {
    /* solid; all vanish at once via g_ghost_age (no fade) */
    fill(x, y, BAR_W, BAR_H, 180,180,180);
}

static void draw_seg(Seg *s) {
    Col c;
    const char *ch;
    int py = s->y / 256;

    if (s->kind == SEG_RED) {
        int fl = (g_animTick/4)%2;
        c.r = fl?220:120; c.g = 0; c.b = 0;
        ch = "!";
    } else if (s->kind == SEG_RANDOM) {
        c = cycle_col(); ch = "?";
    } else if (s->kind == SEG_GREY) {
        c = seg_col(SEG_GREY); ch = "0";
    } else if (s->kind == SEG_PINK) {
        c = seg_col(SEG_PINK); ch = "-";
    } else {
        c = seg_col(s->kind); ch = NULL;
    }

    fill(s->x, py, 10, 16, c.r, c.g, c.b);
    if (ch) rtxt(s->x+1, py, ch, 255,255,255);
}

static void draw_bsod(void) {
    fill(0,0,WIN_W,WIN_H,0,0,170);
    rtxt(50,80, "A fatal exception 0E has occurred at 0028:C000034F in",255,255,255);
    rtxt(50,98, "VXD VMM(01) + 00010E36. The current application will be",255,255,255);
    rtxt(50,116,"terminated.",255,255,255);
    rtxt(50,152,"*  Press any key to terminate the current application.",255,255,255);
    rtxt(50,170,"*  Press CTRL+ALT+DEL again to restart your computer.",255,255,255);
    rtxt(50,188,"   You will lose any unsaved information in all",255,255,255);
    rtxt(50,206,"   applications.",255,255,255);
    rtxt(50,250,"Press any key to continue _",255,255,255);
}

static void draw_gameover(void) {
    char msg[128];
    fill(0,0,WIN_W,WIN_H,0,0,0);
    rtxt_lg(WIN_W/2-60,WIN_H/2-30,"GAME OVER",255,255,255);
    sprintf(msg,"Score: %d   Level: %d",g_score,g_level);
    rtxt(WIN_W/2-70,WIN_H/2+10,msg,255,255,255);
    rtxt(WIN_W/2-90,WIN_H/2+40,"Press ENTER to play again",255,255,255);
}

static void render(void) {
    int i;
    char hud[256];

    SDL_RenderClear(g_ren);

    if (g_bsod)     { draw_bsod();     SDL_RenderPresent(g_ren); return; }
    if (g_gameOver) { draw_gameover(); SDL_RenderPresent(g_ren); return; }

    fill(0,0,WIN_W,AREA_H, 0,128,128);

    for (i = 0; i < g_gCount; i++) {
        int idx = (g_gHead-1-i+MAX_GHOSTS)%MAX_GHOSTS;
        draw_ghost(g_ghosts[idx].x, g_ghosts[idx].y);
    }
    for (i = 0; i < NUM_SEGS; i++)
        if (g_segs[i].alive && g_segs[i].y/256 >= 0) draw_seg(&g_segs[i]);
    draw_bar(g_barX, g_barY);

    fill(0,AREA_H,WIN_W,22, 192,192,192);
    sprintf(hud,"Level: %d   Score: %d   Lives: %d   %s",
        g_level,g_score,g_lives,g_perfectionist?"[Perfectionist]":"");
    rtxt(6,AREA_H+3,hud,0,0,0);

    SDL_RenderPresent(g_ren);
}

static TTF_Font *try_font(int sz) {
    const char *paths[] = {
        "/usr/share/fonts/truetype/dejavu/DejaVuSansMono.ttf",
        "/usr/share/fonts/TTF/DejaVuSansMono.ttf",
        "/usr/share/fonts/dejavu/DejaVuSansMono.ttf",
        "/usr/share/fonts/truetype/liberation/LiberationMono-Regular.ttf",
        "/System/Library/Fonts/Menlo.ttc",
        "C:\\Windows\\Fonts\\cour.ttf",
        NULL
    };
    int i; TTF_Font *f;
    for (i=0; paths[i]; i++) { f=TTF_OpenFont(paths[i],sz); if(f) return f; }
    return NULL;
}

int main(int argc, char *argv[]) {
    SDL_Event e;
    Uint32 last, now;
    int running = 1;
    (void)argc; (void)argv;

    SDL_Init(SDL_INIT_VIDEO);
    TTF_Init();
    g_win = SDL_CreateWindow("ProgressBar95",
        SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
        WIN_W, WIN_H, SDL_WINDOW_SHOWN);
    g_ren = SDL_CreateRenderer(g_win,-1,
        SDL_RENDERER_ACCELERATED|SDL_RENDERER_PRESENTVSYNC);
    g_font   = try_font(13);
    g_fontLg = try_font(28);
    game_init();
    last = SDL_GetTicks();

    while (running) {
        while (SDL_PollEvent(&e)) {
            if (e.type==SDL_QUIT) { running=0; break; }
            if (e.type==SDL_KEYDOWN) {
                if (e.key.keysym.sym==SDLK_ESCAPE) { running=0; break; }
                if (g_bsod) game_dismiss_bsod();
                else if (g_gameOver && e.key.keysym.sym==SDLK_RETURN) game_restart();
            }
            if (e.type==SDL_MOUSEBUTTONDOWN && e.button.button==SDL_BUTTON_LEFT) {
                if (g_bsod) game_dismiss_bsod();
                else game_click(e.button.x, e.button.y);
            }
            if (e.type==SDL_MOUSEBUTTONUP && e.button.button==SDL_BUTTON_LEFT)
                game_release();
            if (e.type==SDL_MOUSEMOTION)
                game_drag(e.motion.x, e.motion.y);
        }
        now = SDL_GetTicks();
        if (now-last >= (Uint32)TICK_MS) {
            last = now;
            if (!g_gameOver) game_tick();
        }
        render();
    }

    if (g_fontLg) TTF_CloseFont(g_fontLg);
    if (g_font)   TTF_CloseFont(g_font);
    SDL_DestroyRenderer(g_ren);
    SDL_DestroyWindow(g_win);
    TTF_Quit();
    SDL_Quit();
    return 0;
}
