/*
 * ProgressBar95 - Win32 C recreation for Windows 95/98/ME
 * Compile: gcc -o progressbar95.exe progressbar95.c -lgdi32 -luser32 -mwindows -std=c89
 * Or with MSVC: cl progressbar95.c user32.lib gdi32.lib /subsystem:windows
 *
 * Controls: Arrow keys to steer the progress bar
 *
 * Dots:
 *   Blue   (+5%, perfectionist)
 *   Orange (+5%, NOT perfectionist)
 *   Pink   (-5%, NOT perfectionist)
 *   Grey   (nothing, shows "0")
 *   Red    (BSOD - game over, shows "!", flashes)
 *   Purple (Random - cycles through all colors, shows "?")
 *   Green  (instantly fills to 100%)
 */

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <stdio.h>

#define TIMER_ID   1
#define TIMER_MS   40

#define AREA_W     640
#define AREA_H     460
#define WIN_W      (AREA_W + 16)
#define WIN_H      (AREA_H + 58)

#define BAR_W      130
#define BAR_H      22

#define MAX_GHOSTS 80
#define NUM_DOTS   16

typedef enum { DOT_BLUE, DOT_ORANGE, DOT_PINK, DOT_GREY, DOT_RED, DOT_RANDOM, DOT_GREEN } DotKind;

typedef struct { int x, y; } Ghost;

typedef struct {
    int x, y;
    DotKind kind;
    BOOL alive;
} Dot;

static HWND   g_hwnd;
static int    g_barX, g_barY;
static int    g_animTick = 0;
static int    g_dx, g_dy;
static Ghost  g_ghosts[MAX_GHOSTS];
static int    g_gHead, g_gCount, g_gTimer;
static int    g_progress;
static BOOL   g_perfectionist;
static int    g_lives;
static Dot    g_dots[NUM_DOTS];
static BOOL   g_gameOver;
static BOOL   g_bsod;
static BOOL   g_win;
static int    g_level;
static int    g_score;

static COLORREF DotColor(DotKind k) {
    switch (k) {
        case DOT_BLUE:   return RGB(0, 80, 220);
        case DOT_ORANGE: return RGB(255, 140, 0);
        case DOT_PINK:   return RGB(255, 50, 150);
        case DOT_GREY:   return RGB(150, 150, 150);
        case DOT_RED:    return RGB(200, 0, 0);
        case DOT_RANDOM: return RGB(180, 0, 200);
        case DOT_GREEN:  return RGB(0, 180, 0);
        default:         return RGB(255, 255, 255);
    }
}

static DotKind RandDotKind(void) {
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
    g_dx = 3;
    g_dy = 0;
    g_gHead = 0;
    g_gCount = 0;
    g_gTimer = 0;
}

static void SpawnDots(void) {
    int i;
    for (i = 0; i < NUM_DOTS; i++) {
        g_dots[i].x = 20 + rand() % (AREA_W - 50);
        g_dots[i].y = 20 + rand() % (AREA_H - 50);
        g_dots[i].kind = RandDotKind();
        g_dots[i].alive = TRUE;
    }
}

static void StartGame(void) {
    g_progress    = 0;
    g_perfectionist = TRUE;
    g_lives       = 2;
    g_level       = 1;
    g_score       = 0;
    g_gameOver    = FALSE;
    g_bsod        = FALSE;
    g_win         = FALSE;
    ResetBar();
    SpawnDots();
}

static void ApplyDot(DotKind k) {
    if (k == DOT_RANDOM) {
        k = (DotKind)(rand() % 7);
    }
    switch (k) {
        case DOT_BLUE:
            g_progress += 5;
            g_score += 10;
            break;
        case DOT_ORANGE:
            g_progress += 5;
            g_perfectionist = FALSE;
            g_score += 5;
            break;
        case DOT_PINK:
            g_progress -= 5;
            if (g_progress < 0) g_progress = 0;
            g_perfectionist = FALSE;
            break;
        case DOT_GREY:
            break;
        case DOT_RED:
            g_bsod = TRUE;
            g_gameOver = TRUE;
            break;
        case DOT_GREEN:
            g_progress = 100;
            g_score += 50;
            break;
        default:
            break;
    }
    if (g_progress > 100) g_progress = 100;
}

static void GameTick(void) {
    int i;
    RECT barR, dotR, inter;

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

    barR.left   = g_barX;
    barR.top    = g_barY;
    barR.right  = g_barX + BAR_W;
    barR.bottom = g_barY + BAR_H;

    for (i = 0; i < NUM_DOTS; i++) {
        if (!g_dots[i].alive) continue;
        dotR.left   = g_dots[i].x;
        dotR.top    = g_dots[i].y;
        dotR.right  = g_dots[i].x + 16;
        dotR.bottom = g_dots[i].y + 16;
        if (IntersectRect(&inter, &barR, &dotR)) {
            g_dots[i].alive = FALSE;
            ApplyDot(g_dots[i].kind);
            if (g_gameOver) return;
        }
    }

    if (g_progress >= 100) {
        g_level++;
        g_progress = 0;
        g_score += 100 + (g_perfectionist ? 50 : 0);
        g_perfectionist = TRUE;
        ResetBar();
        SpawnDots();
        return;
    }

    for (i = 0; i < NUM_DOTS; i++) {
        if (!g_dots[i].alive && rand() % 300 == 0) {
            g_dots[i].x = 20 + rand() % (AREA_W - 50);
            g_dots[i].y = 20 + rand() % (AREA_H - 50);
            g_dots[i].kind = RandDotKind();
            g_dots[i].alive = TRUE;
        }
    }

    InvalidateRect(g_hwnd, NULL, FALSE);
}

static void DrawBar(HDC hdc, int x, int y, int prog) {
    RECT r;
    HBRUSH br;
    HPEN pen, oldPen;
    HBRUSH oldBr;
    int fillW;
    char pct[8];

    pen = CreatePen(PS_SOLID, 1, RGB(0, 0, 0));
    oldPen = (HPEN)SelectObject(hdc, pen);
    br = CreateSolidBrush(RGB(192, 192, 192));
    oldBr = (HBRUSH)SelectObject(hdc, br);
    Rectangle(hdc, x, y, x + BAR_W, y + BAR_H);
    SelectObject(hdc, oldBr);
    SelectObject(hdc, oldPen);
    DeleteObject(br);
    DeleteObject(pen);

    fillW = prog * (BAR_W - 4) / 100;
    if (fillW > 0) {
        br = CreateSolidBrush(RGB(0, 0, 128));
        r.left = x + 2; r.top = y + 2;
        r.right = x + 2 + fillW; r.bottom = y + BAR_H - 2;
        FillRect(hdc, &r, br);
        DeleteObject(br);
    }

    sprintf(pct, "%d%%", prog);
    SetBkMode(hdc, TRANSPARENT);
    SetTextColor(hdc, fillW > BAR_W / 2 ? RGB(255, 255, 255) : RGB(0, 0, 0));
    TextOut(hdc, x + BAR_W / 2 - 12, y + 3, pct, lstrlen(pct));
}

static void DrawGhostBar(HDC hdc, int x, int y, int idx, int total) {
    HPEN pen;
    HPEN oldPen;
    HBRUSH br;
    HBRUSH oldBr;
    int fade = 200 - (idx * 200 / (total > 1 ? total : 1));
    COLORREF c;

    if (fade < 20) fade = 20;
    c = RGB(fade, fade, fade);
    pen = CreatePen(PS_SOLID, 1, c);
    oldPen = (HPEN)SelectObject(hdc, pen);
    br = CreateSolidBrush(c);
    oldBr = (HBRUSH)SelectObject(hdc, br);
    Rectangle(hdc, x, y, x + BAR_W, y + BAR_H);
    SelectObject(hdc, oldBr);
    SelectObject(hdc, oldPen);
    DeleteObject(br);
    DeleteObject(pen);
}

static COLORREF RandomCycleColor(void) {
    static COLORREF cycle[] = {
        RGB(0, 80, 220), RGB(255, 140, 0), RGB(255, 50, 150),
        RGB(150, 150, 150), RGB(200, 0, 0), RGB(0, 180, 0)
    };
    return cycle[(g_animTick / 6) % 6];
}

static void DrawDot(HDC hdc, Dot *d) {
    RECT r;
    HBRUSH br;
    COLORREF bg;
    const char *ch = "";
    BOOL flash_visible = TRUE;

    if (d->kind == DOT_RED) {
        flash_visible = (g_animTick / 4) % 2 == 0;
        bg = flash_visible ? RGB(200, 0, 0) : RGB(120, 0, 0);
    } else if (d->kind == DOT_RANDOM) {
        bg = RandomCycleColor();
    } else {
        bg = DotColor(d->kind);
    }

    br = CreateSolidBrush(bg);
    r.left = d->x; r.top = d->y;
    r.right = d->x + 16; r.bottom = d->y + 16;
    FillRect(hdc, &r, br);
    DeleteObject(br);

    br = CreateSolidBrush(RGB(0, 0, 0));
    FrameRect(hdc, &r, br);
    DeleteObject(br);

    switch (d->kind) {
        case DOT_GREY:   ch = "0"; break;
        case DOT_RED:    ch = "!"; break;
        case DOT_RANDOM: ch = "?"; break;
        default:         ch = "";  break;
    }

    if (ch[0] != '\0') {
        SetBkMode(hdc, TRANSPARENT);
        SetTextColor(hdc, d->kind == DOT_GREY ? RGB(50,50,50) : RGB(255,255,255));
        TextOut(hdc, d->x + 3, d->y + 1, ch, 1);
    }
}

static void DrawBSOD(HDC hdc) {
    RECT r;
    HBRUSH br;
    HFONT font, oldFont;

    br = CreateSolidBrush(RGB(0, 0, 170));
    r.left = 0; r.top = 0; r.right = AREA_W; r.bottom = AREA_H + 20;
    FillRect(hdc, &r, br);
    DeleteObject(br);

    font = CreateFont(16, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
        ANSI_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
        DEFAULT_QUALITY, FIXED_PITCH | FF_MODERN, "Courier New");
    oldFont = (HFONT)SelectObject(hdc, font);

    SetTextColor(hdc, RGB(255, 255, 255));
    SetBkColor(hdc, RGB(0, 0, 170));
    SetBkMode(hdc, OPAQUE);

    TextOut(hdc, 50, 80,  "A fatal exception 0E has occurred at 0028:C000034F in", 53);
    TextOut(hdc, 50, 98,  "VXD VMM(01) + 00010E36. The current application will be", 55);
    TextOut(hdc, 50, 116, "terminated.", 11);
    TextOut(hdc, 50, 152, "*  Press any key to terminate the current application.", 54);
    TextOut(hdc, 50, 170, "*  Press CTRL+ALT+DEL again to restart your computer.", 53);
    TextOut(hdc, 50, 188, "   You will lose any unsaved information in all", 47);
    TextOut(hdc, 50, 206, "   applications.", 16);
    TextOut(hdc, 50, 250, "Press any key to continue _", 27);

    SelectObject(hdc, oldFont);
    DeleteObject(font);
}

static void DrawGameOver(HDC hdc) {
    RECT r;
    HBRUSH br;
    char msg[128];

    br = CreateSolidBrush(RGB(0, 0, 0));
    r.left = 0; r.top = 0; r.right = AREA_W; r.bottom = AREA_H + 20;
    FillRect(hdc, &r, br);
    DeleteObject(br);

    SetTextColor(hdc, RGB(255, 255, 255));
    SetBkMode(hdc, OPAQUE);
    SetBkColor(hdc, RGB(0, 0, 0));

    TextOut(hdc, AREA_W/2 - 50, AREA_H/2 - 20, "GAME OVER", 9);
    sprintf(msg, "Score: %d  Level: %d", g_score, g_level);
    TextOut(hdc, AREA_W/2 - 60, AREA_H/2 + 10, msg, lstrlen(msg));
    TextOut(hdc, AREA_W/2 - 70, AREA_H/2 + 40, "Press ENTER to play again", 25);
}

static void OnPaint(HWND hwnd) {
    PAINTSTRUCT ps;
    HDC hdc, memDC;
    HBITMAP memBmp, oldBmp;
    RECT clientR;
    HBRUSH bg;
    char hud[256];
    int i;

    hdc = BeginPaint(hwnd, &ps);
    GetClientRect(hwnd, &clientR);

    memDC = CreateCompatibleDC(hdc);
    memBmp = CreateCompatibleBitmap(hdc, clientR.right, clientR.bottom);
    oldBmp = (HBITMAP)SelectObject(memDC, memBmp);

    if (g_bsod) {
        DrawBSOD(memDC);
    } else if (g_gameOver) {
        DrawGameOver(memDC);
    } else {
        /* Background */
        bg = CreateSolidBrush(RGB(0, 128, 128));
        FillRect(memDC, &clientR, bg);
        DeleteObject(bg);

        /* Ghosts - oldest first */
        for (i = g_gCount - 1; i >= 0; i--) {
            int idx = (g_gHead - 1 - i + MAX_GHOSTS) % MAX_GHOSTS;
            DrawGhostBar(memDC, g_ghosts[idx].x, g_ghosts[idx].y, i, g_gCount);
        }

        /* Dots */
        for (i = 0; i < NUM_DOTS; i++) {
            if (g_dots[i].alive) DrawDot(memDC, &g_dots[i]);
        }

        /* Main bar */
        DrawBar(memDC, g_barX, g_barY, g_progress);

        /* HUD bar at bottom */
        bg = CreateSolidBrush(RGB(192, 192, 192));
        clientR.top = clientR.bottom - 22;
        FillRect(memDC, &clientR, bg);
        DeleteObject(bg);

        sprintf(hud, "Level: %d   Score: %d   Lives: %d   %s",
            g_level, g_score, g_lives,
            g_perfectionist ? "[Perfectionist]" : "");
        SetBkColor(memDC, RGB(192, 192, 192));
        SetTextColor(memDC, RGB(0, 0, 0));
        SetBkMode(memDC, OPAQUE);
        TextOut(memDC, 6, clientR.top + 3, hud, lstrlen(hud));
    }

    /* Blit to screen */
    GetClientRect(hwnd, &clientR);
    BitBlt(hdc, 0, 0, clientR.right, clientR.bottom, memDC, 0, 0, SRCCOPY);

    SelectObject(memDC, oldBmp);
    DeleteObject(memBmp);
    DeleteDC(memDC);
    EndPaint(hwnd, &ps);
}

LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
        case WM_CREATE:
            srand((unsigned int)time(NULL));
            StartGame();
            SetTimer(hwnd, TIMER_ID, TIMER_MS, NULL);
            return 0;

        case WM_TIMER:
            if (!g_gameOver) GameTick();
            return 0;

        case WM_KEYDOWN:
            if (g_bsod) {
                g_bsod = FALSE;
                g_gameOver = FALSE;
                g_lives--;
                if (g_lives < 0) {
                    g_gameOver = TRUE;
                } else {
                    ResetBar();
                    SpawnDots();
                }
                InvalidateRect(hwnd, NULL, FALSE);
                return 0;
            }
            if (g_gameOver) {
                if (wParam == VK_RETURN) {
                    StartGame();
                    InvalidateRect(hwnd, NULL, FALSE);
                }
                return 0;
            }
            switch (wParam) {
                case VK_LEFT:  if (g_dx == 0) { g_dx = -3; g_dy = 0; } break;
                case VK_RIGHT: if (g_dx == 0) { g_dx =  3; g_dy = 0; } break;
                case VK_UP:    if (g_dy == 0) { g_dy = -3; g_dx = 0; } break;
                case VK_DOWN:  if (g_dy == 0) { g_dy =  3; g_dx = 0; } break;
            }
            return 0;

        case WM_PAINT:
            OnPaint(hwnd);
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
    wc.style         = CS_HREDRAW | CS_VREDRAW;
    wc.lpfnWndProc   = WndProc;
    wc.hInstance     = hInst;
    wc.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
    wc.lpszClassName = "ProgressBar95";
    wc.hCursor       = LoadCursor(NULL, IDC_ARROW);
    wc.hIcon         = LoadIcon(NULL, IDI_APPLICATION);
    RegisterClass(&wc);

    g_hwnd = CreateWindow("ProgressBar95", "ProgressBar95",
        WS_OVERLAPPEDWINDOW | WS_VISIBLE,
        80, 60, WIN_W, WIN_H,
        NULL, NULL, hInst, NULL);

    if (!g_hwnd) return 1;

    ShowWindow(g_hwnd, nShow);
    UpdateWindow(g_hwnd);

    while (GetMessage(&msg, NULL, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }
    return (int)msg.wParam;
}
