# progressbar95-win95

A Win32 C recreation of [progressbar95](https://progressbar95.woloweb.io/) that runs on **Windows 95/98/ME**.

Pure Win32 API + GDI, zero modern dependencies.

## How to compile

**MinGW (cross-compile from Linux/Mac):**
```
i686-w64-mingw32-gcc -o progressbar95.exe progressbar95.c -lgdi32 -luser32 -mwindows -std=c89
```

**MinGW on Windows:**
```
gcc -o progressbar95.exe progressbar95.c -lgdi32 -luser32 -mwindows -std=c89
```

**MSVC:**
```
cl progressbar95.c user32.lib gdi32.lib /subsystem:windows
```

## Controls

Arrow keys to steer the progress bar.

## Dots

| Dot | Color | Effect |
|-----|-------|--------|
| Blue | Blue | +5% progress (perfectionist) |
| Orange | Orange | +5% progress (NOT perfectionist) |
| Pink | Pink | -5% progress (NOT perfectionist) |
| Grey `0` | Grey | Nothing |
| Red `!` | Red (flashing) | BSOD - lose a life |
| Purple `?` | Cycles all colors | Random - could be anything including Green |
| Green | Green | Instantly fills to 100% |

## Goal

Fill the progress bar to 100% collecting only blue dots to maintain **[Perfectionist]** status. Complete levels to increase score.

Made by [@oddsclaude](https://github.com/oddsclaude)
