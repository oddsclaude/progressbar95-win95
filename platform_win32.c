/*
 * platform_win32.c - Win32/GDI renderer for ProgressBar95
 * Targets Windows 95/98/ME - pure Win32 API, no modern deps
 *
 * Compile: gcc -o progressbar95.exe platform_win32.c game.c -lgdi32 -luser32 -mwindows -std=c89
 * Or: cl platform_win32.c game.c user32.lib gdi32.lib /subsystem:windows
 */

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <stdio.h>
#include <string.h>
#include "game.h"

#define TIMER_ID  1
#define TIMER_MS  40
#define WIN_W     (AREA_W + 16)
#define WIN_H     (AREA_H + 58)

static HWND g_hwnd;

/* ---- rendering helpers ---- */

static void draw_bar(HDC hdc, int x, int y, int prog) {
    RECT r;
    HBRUSH br;
    HPEN pen, oldPen;
    HBRUSH oldBr;
    int fillW = prog * (BAR_W - 4) / 100;
    char pct[8];

    pen = CreatePen(PS_SOLID, 1, RGB(0,0,0));
    oldPen = (HPEN)SelectObject(hdc, pen);
    br = CreateSolidBrush(RGB(192,192,192));
    oldBr = (HBRUSH)SelectObject(hdc, br);
    Rectangle(hdc, x, y, x+BAR_W, y+BAR_H);
    SelectObject(hdc, oldBr); SelectObject(hdc, oldPen);
    DeleteObject(br); DeleteObject(pen);

    if (fillW > 0) {
        br = CreateSolidBrush(RGB(0,0,128));
        r.left = x+2; r.top = y+2; r.right = x+2+fillW; r.bottom = y+BAR_H-2;
        FillRect(hdc, &r, br);
        DeleteObject(br);
    }

    sprintf(pct, "%d%%", prog);
    SetBkMode(hdc, TRANSPARENT);
    SetTextColor(hdc, fillW > BAR_W/2 ? RGB(255,255,255) : RGB(0,0,0));
    TextOut(hdc, x+BAR_W/2-12, y+3, pct, lstrlen(pct));
}

static void draw_ghost(HDC hdc, int x, int y, int idx, int total) {
    HPEN pen, oldPen;
    HBRUSH br, oldBr;
    int fade = 200 - (idx * 200 / (total > 1 ? total : 1));
    COLORREF c;
    if (fade < 20) fade = 20;
    c = RGB(fade,fade,fade);
    pen = CreatePen(PS_SOLID,1,c); oldPen = (HPEN)SelectObject(hdc,pen);
    br = CreateSolidBrush(c);     oldBr  = (HBRUSH)SelectObject(hdc,br);
    Rectangle(hdc, x, y, x+BAR_W, y+BAR_H);
    SelectObject(hdc,oldBr); SelectObject(hdc,oldPen);
    DeleteObject(br); DeleteObject(pen);
}

static COLORREF dot_color(DotKind k) {
    switch (k) {
        case DOT_BLUE:   return RGB(0,80,220);
        case DOT_ORANGE: return RGB(255,140,0);
        case DOT_PINK:   return RGB(255,50,150);
        case DOT_GREY:   return RGB(150,150,150);
        case DOT_RED:    return RGB(200,0,0);
        case DOT_RANDOM: return RGB(180,0,200);
        case DOT_GREEN:  return RGB(0,180,0);
        default:         return RGB(255,255,255);
    }
}

static COLORREF cycle_color(void) {
    static COLORREF c[] = {
        RGB(0,80,220),RGB(255,140,0),RGB(255,50,150),
        RGB(150,150,150),RGB(200,0,0),RGB(0,180,0)
    };
    return c[(g_animTick/6)%6];
}

static void draw_dot(HDC hdc, Dot *d) {
    COLORREF c;
    const char *ch;
    int py = d->y / 256;

    if (d->kind == DOT_RED) {
        c = (g_animTick/4)%2 ? RGB(200,0,0) : RGB(120,0,0);
        ch = "!";
    } else if (d->kind == DOT_RANDOM) {
        c = cycle_color();
        ch = "?";
    } else if (d->kind == DOT_GREY) {
        c = dot_color(DOT_GREY); ch = "0";
    } else if (d->kind == DOT_BLUE) {
        c = dot_color(DOT_BLUE); ch = "B";
    } else if (d->kind == DOT_ORANGE) {
        c = dot_color(DOT_ORANGE); ch = "O";
    } else if (d->kind == DOT_PINK) {
        c = dot_color(DOT_PINK); ch = "P";
    } else {
        c = dot_color(d->kind); ch = "G";
    }

    /* VGA text mode style - character only, no background box */
    SetBkMode(hdc, TRANSPARENT);
    SetTextColor(hdc, c);
    TextOut(hdc, d->x, py, ch, 1);
}

static void draw_bsod(HDC hdc) {
    RECT r;
    HBRUSH br;
    HFONT font, oldFont;

    br = CreateSolidBrush(RGB(0,0,170));
    r.left=0; r.top=0; r.right=AREA_W; r.bottom=AREA_H+20;
    FillRect(hdc,&r,br); DeleteObject(br);

    font = CreateFont(16,0,0,0,FW_NORMAL,FALSE,FALSE,FALSE,
        ANSI_CHARSET,OUT_DEFAULT_PRECIS,CLIP_DEFAULT_PRECIS,
        DEFAULT_QUALITY,FIXED_PITCH|FF_MODERN,"Courier New");
    oldFont = (HFONT)SelectObject(hdc, font);
    SetTextColor(hdc, RGB(255,255,255));
    SetBkColor(hdc, RGB(0,0,170));
    SetBkMode(hdc, OPAQUE);

    TextOut(hdc,50,80, "A fatal exception 0E has occurred at 0028:C000034F in",53);
    TextOut(hdc,50,98, "VXD VMM(01) + 00010E36. The current application will be",55);
    TextOut(hdc,50,116,"terminated.",11);
    TextOut(hdc,50,152,"*  Press any key to terminate the current application.",54);
    TextOut(hdc,50,170,"*  Press CTRL+ALT+DEL again to restart your computer.",53);
    TextOut(hdc,50,188,"   You will lose any unsaved information in all",47);
    TextOut(hdc,50,206,"   applications.",16);
    TextOut(hdc,50,250,"Press any key to continue _",27);

    SelectObject(hdc, oldFont); DeleteObject(font);
}

static void draw_gameover(HDC hdc) {
    RECT r;
    HBRUSH br;
    char msg[128];

    br = CreateSolidBrush(RGB(0,0,0));
    r.left=0; r.top=0; r.right=AREA_W; r.bottom=AREA_H+20;
    FillRect(hdc,&r,br); DeleteObject(br);

    SetTextColor(hdc,RGB(255,255,255));
    SetBkMode(hdc,OPAQUE); SetBkColor(hdc,RGB(0,0,0));
    TextOut(hdc,AREA_W/2-50,AREA_H/2-20,"GAME OVER",9);
    sprintf(msg,"Score: %d  Level: %d",g_score,g_level);
    TextOut(hdc,AREA_W/2-60,AREA_H/2+10,msg,lstrlen(msg));
    TextOut(hdc,AREA_W/2-70,AREA_H/2+40,"Press ENTER to play again",25);
}

static void on_paint(HWND hwnd) {
    PAINTSTRUCT ps;
    HDC hdc, memDC;
    HBITMAP bmp, oldBmp;
    RECT clientR;
    HBRUSH bg;
    char hud[256];
    int i;

    hdc = BeginPaint(hwnd, &ps);
    GetClientRect(hwnd, &clientR);
    memDC = CreateCompatibleDC(hdc);
    bmp = CreateCompatibleBitmap(hdc, clientR.right, clientR.bottom);
    oldBmp = (HBITMAP)SelectObject(memDC, bmp);

    if (g_bsod) {
        draw_bsod(memDC);
    } else if (g_gameOver) {
        draw_gameover(memDC);
    } else {
        bg = CreateSolidBrush(RGB(0,128,128));
        FillRect(memDC, &clientR, bg); DeleteObject(bg);

        for (i = g_gCount-1; i >= 0; i--) {
            int idx = (g_gHead-1-i+MAX_GHOSTS)%MAX_GHOSTS;
            draw_ghost(memDC, g_ghosts[idx].x, g_ghosts[idx].y, i, g_gCount);
        }
        for (i = 0; i < NUM_DOTS; i++)
            if (g_dots[i].alive) draw_dot(memDC, &g_dots[i]);
        draw_bar(memDC, g_barX, g_barY, g_progress);

        bg = CreateSolidBrush(RGB(192,192,192));
        clientR.top = clientR.bottom - 22;
        FillRect(memDC, &clientR, bg); DeleteObject(bg);

        sprintf(hud, "Level: %d   Score: %d   Lives: %d   %s",
            g_level, g_score, g_lives,
            g_perfectionist ? "[Perfectionist]" : "");
        SetBkColor(memDC,RGB(192,192,192));
        SetTextColor(memDC,RGB(0,0,0));
        SetBkMode(memDC,OPAQUE);
        TextOut(memDC, 6, clientR.top+3, hud, lstrlen(hud));
    }

    GetClientRect(hwnd, &clientR);
    BitBlt(hdc,0,0,clientR.right,clientR.bottom,memDC,0,0,SRCCOPY);
    SelectObject(memDC,oldBmp); DeleteObject(bmp); DeleteDC(memDC);
    EndPaint(hwnd, &ps);
}

/* ---- window proc ---- */

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
            if (g_bsod) {
                game_dismiss_bsod();
            } else if (g_gameOver) {
                /* nothing on click */
            } else {
                game_click(LOWORD(lParam), HIWORD(lParam));
                SetCapture(hwnd);
            }
            return 0;

        case WM_MOUSEMOVE:
            game_drag(LOWORD(lParam), HIWORD(lParam));
            return 0;

        case WM_LBUTTONUP:
            game_release();
            ReleaseCapture();
            return 0;

        case WM_KEYDOWN:
            if (g_bsod) {
                game_dismiss_bsod();
            } else if (g_gameOver && wParam == VK_RETURN) {
                game_restart();
            }
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
    (void)hPrev; (void)lpCmd;

    memset(&wc, 0, sizeof(wc));
    wc.style         = CS_HREDRAW|CS_VREDRAW;
    wc.lpfnWndProc   = WndProc;
    wc.hInstance     = hInst;
    wc.hbrBackground = (HBRUSH)(COLOR_WINDOW+1);
    wc.lpszClassName = "ProgressBar95";
    wc.hCursor       = LoadCursor(NULL, IDC_ARROW);
    wc.hIcon         = LoadIcon(NULL, IDI_APPLICATION);
    RegisterClass(&wc);

    g_hwnd = CreateWindow("ProgressBar95","ProgressBar95",
        WS_OVERLAPPEDWINDOW|WS_VISIBLE,
        80,60,WIN_W,WIN_H,NULL,NULL,hInst,NULL);
    if (!g_hwnd) return 1;

    ShowWindow(g_hwnd, nShow);
    UpdateWindow(g_hwnd);

    while (GetMessage(&msg,NULL,0,0)) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }
    return (int)msg.wParam;
}
