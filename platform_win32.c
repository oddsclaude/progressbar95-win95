/*
 * platform_win32.c - Win32/GDI renderer for ProgressBar95
 * Transparent overlay: teal (0,128,128) is colorkey -> shows OS desktop.
 * Right-click or ESC to quit.
 *
 * Compile: i686-w64-mingw32-gcc -o progressbar95.exe platform_win32.c game.c -lgdi32 -luser32 -mwindows -std=c89
 */

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <stdio.h>
#include <string.h>
#include "game.h"

#define TIMER_ID 1
#define TIMER_MS 40
#define HUD_H    22   /* HUD strip at top of window */
#define WIN_W    AREA_W
#define WIN_H    (AREA_H + HUD_H)
/* teal = colorkey (transparent) */
#define TEAL     RGB(0,128,128)

static HWND g_hwnd;

static COLORREF seg_color(SegKind k) {
    switch (k) {
        case SEG_BLUE:   return RGB(  0,  0,170);
        case SEG_YELLOW: return RGB(255,140,  0);
        case SEG_PINK:   return RGB(255,  0,200);
        case SEG_GREY:   return RGB(170,170,170);
        case SEG_RED:    return RGB(220,  0,  0);
        case SEG_GREEN:  return RGB(  0,200,  0);
        case SEG_CYAN:   return RGB(  0,190,255);
        case SEG_RANDOM: return RGB(255,255,255);
        default:         return RGB(255,255,255);
    }
}

static COLORREF cycle_color(void) {
    static COLORREF c[] = {
        RGB(0,0,170),RGB(255,140,0),RGB(255,0,200),
        RGB(170,170,170),RGB(220,0,0),RGB(0,200,0)
    };
    return c[(g_animTick/5)%6];
}

static void fill_rect(HDC hdc, int x, int y, int w, int h, COLORREF c) {
    RECT r;
    HBRUSH br;
    r.left=x; r.top=y; r.right=x+w; r.bottom=y+h;
    br = CreateSolidBrush(c);
    FillRect(hdc, &r, br);
    DeleteObject(br);
}

static void draw_bar(HDC hdc, int x, int y) {
    /* y is game-space; add HUD_H for window coords */
    int wy = y + HUD_H;
    int i, seg_w, sx, fill_end;
    char pct[20];
    RECT r;

    seg_w = (BAR_W - 4) / MAX_SEGS;

    /* interior background */
    fill_rect(hdc, x, wy, BAR_W, BAR_H, RGB(212,208,200));

    /* segment blocks */
    if (!g_null_active) {
        for (i = 0; i < g_bar_seg_count; i++) {
            HBRUSH br;
            sx = x + 2 + i * seg_w;
            r.left=sx; r.top=wy+2; r.right=sx+seg_w-1; r.bottom=wy+BAR_H-2;
            br = CreateSolidBrush(seg_color(g_bar_segs[i]));
            FillRect(hdc, &r, br);
            DeleteObject(br);
        }
    }
    (void)fill_end;

    /* Win95 sunken border */
    r.left=x; r.top=wy; r.right=x+BAR_W; r.bottom=wy+BAR_H;
    DrawEdge(hdc, &r, EDGE_SUNKEN, BF_RECT);

    /* label */
    if (g_bar_label[0]) {
        lstrcpyn(pct, g_bar_label, sizeof(pct));
    } else {
        sprintf(pct, "%d %%", g_progress);
    }
    SetBkMode(hdc, TRANSPARENT);
    SetTextColor(hdc, RGB(0,0,0));
    TextOut(hdc, x + BAR_W/2 - 16, wy + (BAR_H-16)/2, pct, lstrlen(pct));
}

static void draw_ghost(HDC hdc, int x, int y) {
    /* solid grey; ghosts all vanish at once via g_ghost_age */
    fill_rect(hdc, x, y + HUD_H, BAR_W, BAR_H, RGB(180,180,180));
}

static void draw_seg(HDC hdc, Seg *s) {
    COLORREF c;
    const char *ch;
    RECT r;
    HBRUSH br;
    int py = s->y / 256 + HUD_H;  /* window coords */

    if (py + 16 < HUD_H || py > WIN_H) return;  /* fully clipped */

    if (s->kind == SEG_RED) {
        c  = (g_animTick/4)%2 ? RGB(220,0,0) : RGB(120,0,0);
        ch = "!";
    } else if (s->kind == SEG_RANDOM) {
        c  = cycle_color();
        ch = "?";
    } else if (s->kind == SEG_GREY) {
        c  = seg_color(SEG_GREY); ch = "0";
    } else if (s->kind == SEG_PINK) {
        c  = seg_color(SEG_PINK); ch = "-";
    } else {
        c  = seg_color(s->kind); ch = NULL;
    }

    r.left=s->x; r.top=py; r.right=s->x+10; r.bottom=py+16;
    br = CreateSolidBrush(c);
    FillRect(hdc, &r, br);
    DeleteObject(br);

    if (ch) {
        SetBkMode(hdc, TRANSPARENT);
        SetTextColor(hdc, RGB(255,255,255));
        TextOut(hdc, s->x+1, py, ch, 1);
    }
}

static void draw_bsod(HDC hdc) {
    HFONT font, oldFont;
    fill_rect(hdc, 0, 0, WIN_W, WIN_H, RGB(0,0,170));
    font = CreateFont(16,0,0,0,FW_NORMAL,FALSE,FALSE,FALSE,
        ANSI_CHARSET,OUT_DEFAULT_PRECIS,CLIP_DEFAULT_PRECIS,
        DEFAULT_QUALITY,FIXED_PITCH|FF_MODERN,"Courier New");
    oldFont = (HFONT)SelectObject(hdc, font);
    SetTextColor(hdc,RGB(255,255,255));
    SetBkColor(hdc,RGB(0,0,170));
    SetBkMode(hdc,OPAQUE);
    TextOut(hdc,50,80, "A fatal exception 0E has occurred at 0028:C000034F in",53);
    TextOut(hdc,50,98, "VXD VMM(01) + 00010E36. The current application will be",55);
    TextOut(hdc,50,116,"terminated.",11);
    TextOut(hdc,50,152,"*  Press any key to terminate the current application.",54);
    TextOut(hdc,50,170,"*  Press CTRL+ALT+DEL again to restart your computer.",53);
    TextOut(hdc,50,188,"   You will lose any unsaved information in all",47);
    TextOut(hdc,50,206,"   applications.",16);
    TextOut(hdc,50,250,"Press any key to continue _",27);
    SelectObject(hdc,oldFont); DeleteObject(font);
}

static void draw_gameover(HDC hdc) {
    char msg[128];
    fill_rect(hdc,0,0,WIN_W,WIN_H,RGB(0,0,0));
    SetTextColor(hdc,RGB(255,255,255));
    SetBkMode(hdc,OPAQUE); SetBkColor(hdc,RGB(0,0,0));
    TextOut(hdc,WIN_W/2-50,WIN_H/2-20,"GAME OVER",9);
    sprintf(msg,"Score: %d  Level: %d",g_score,g_level);
    TextOut(hdc,WIN_W/2-60,WIN_H/2+10,msg,lstrlen(msg));
    TextOut(hdc,WIN_W/2-70,WIN_H/2+40,"Press ENTER to play again",25);
}

static void on_paint(HWND hwnd) {
    PAINTSTRUCT ps;
    HDC hdc, memDC;
    HBITMAP bmp, oldBmp;
    RECT clientR;
    char hud[256];
    int i;

    hdc = BeginPaint(hwnd, &ps);
    GetClientRect(hwnd, &clientR);
    memDC  = CreateCompatibleDC(hdc);
    bmp    = CreateCompatibleBitmap(hdc, clientR.right, clientR.bottom);
    oldBmp = (HBITMAP)SelectObject(memDC, bmp);

    if (g_bsod) {
        draw_bsod(memDC);
    } else if (g_gameOver) {
        draw_gameover(memDC);
    } else {
        /* teal = transparent -> shows OS desktop */
        fill_rect(memDC, 0, HUD_H, WIN_W, AREA_H, TEAL);

        /* ghosts (solid, instant-clear) */
        for (i = 0; i < g_gCount; i++) {
            int idx = (g_gHead-1-i+MAX_GHOSTS)%MAX_GHOSTS;
            draw_ghost(memDC, g_ghosts[idx].x, g_ghosts[idx].y);
        }

        /* falling segments */
        for (i = 0; i < NUM_SEGS; i++)
            if (g_segs[i].alive && g_segs[i].y/256 >= 0)
                draw_seg(memDC, &g_segs[i]);

        /* progress bar */
        draw_bar(memDC, g_barX, g_barY);

        /* HUD strip at top (opaque grey) */
        fill_rect(memDC, 0, 0, WIN_W, HUD_H, RGB(192,192,192));
        sprintf(hud, "Level: %d  Score: %d  Lives: %d  %s",
            g_level, g_score, g_lives,
            g_perfectionist ? "[Perfectionist]" : "");
        SetBkColor(memDC,RGB(192,192,192));
        SetTextColor(memDC,RGB(0,0,0));
        SetBkMode(memDC,OPAQUE);
        TextOut(memDC, 6, 3, hud, lstrlen(hud));
    }

    BitBlt(hdc,0,0,clientR.right,clientR.bottom,memDC,0,0,SRCCOPY);
    SelectObject(memDC,oldBmp); DeleteObject(bmp); DeleteDC(memDC);
    EndPaint(hwnd, &ps);
}

LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
        case WM_CREATE:
            game_init();
            SetTimer(hwnd, TIMER_ID, TIMER_MS, NULL);
            return 0;
        case WM_TIMER:
            if (!g_gameOver) game_tick();
            InvalidateRect(hwnd, NULL, FALSE);
            return 0;
        case WM_LBUTTONDOWN:
            if (g_bsod) game_dismiss_bsod();
            else if (!g_gameOver) {
                /* subtract HUD_H to get game-space coords */
                game_click(LOWORD(lParam), HIWORD(lParam) - HUD_H);
                SetCapture(hwnd);
            }
            return 0;
        case WM_MOUSEMOVE:
            game_drag(LOWORD(lParam), HIWORD(lParam) - HUD_H);
            return 0;
        case WM_LBUTTONUP:
            game_release(); ReleaseCapture();
            return 0;
        case WM_RBUTTONDOWN:
            /* right-click to quit (no title bar in overlay mode) */
            PostQuitMessage(0);
            return 0;
        case WM_KEYDOWN:
            if (wParam == VK_ESCAPE) { PostQuitMessage(0); return 0; }
            if (g_bsod) game_dismiss_bsod();
            else if (g_gameOver && wParam==VK_RETURN) game_restart();
            InvalidateRect(hwnd, NULL, FALSE);
            return 0;
        case WM_PAINT:
            on_paint(hwnd);
            return 0;
        case WM_DESTROY:
            KillTimer(hwnd, TIMER_ID);
            PostQuitMessage(0);
            return 0;
    }
    return DefWindowProc(hwnd, msg, wParam, lParam);
}

int WINAPI WinMain(HINSTANCE hInst, HINSTANCE hPrev, LPSTR lpCmd, int nShow) {
    WNDCLASS wc;
    MSG msg;
    int sx, sy;
    (void)hPrev; (void)lpCmd; (void)nShow;

    memset(&wc, 0, sizeof(wc));
    wc.style         = CS_HREDRAW|CS_VREDRAW;
    wc.lpfnWndProc   = WndProc;
    wc.hInstance     = hInst;
    wc.hbrBackground = NULL;  /* we paint everything */
    wc.lpszClassName = "ProgressBar95";
    wc.hCursor       = LoadCursor(NULL, IDC_ARROW);
    wc.hIcon         = LoadIcon(NULL, IDI_APPLICATION);
    RegisterClass(&wc);

    /* center on screen */
    sx = (GetSystemMetrics(SM_CXSCREEN) - WIN_W) / 2;
    sy = (GetSystemMetrics(SM_CYSCREEN) - WIN_H) / 4;

    g_hwnd = CreateWindowEx(
        WS_EX_LAYERED | WS_EX_TOPMOST,
        "ProgressBar95", NULL,
        WS_POPUP | WS_VISIBLE,
        sx, sy, WIN_W, WIN_H,
        NULL, NULL, hInst, NULL
    );
    if (!g_hwnd) return 1;

    /* teal pixels become fully transparent -> desktop shows through */
    SetLayeredWindowAttributes(g_hwnd, TEAL, 0, LWA_COLORKEY);

    while (GetMessage(&msg,NULL,0,0)) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }
    return (int)msg.wParam;
}
