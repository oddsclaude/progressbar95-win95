CC     = gcc
CC_W95 = i686-w64-mingw32-gcc

SDL_FLAGS = $(shell pkg-config --cflags --libs sdl2 SDL2_ttf 2>/dev/null || echo "-lSDL2 -lSDL2_ttf")

.PHONY: all sdl w95 clean

all: sdl

# Linux/Wayland/macOS (SDL2)
sdl: platform_sdl2.c game.c game.h
	$(CC) -o progressbar95 platform_sdl2.c game.c $(SDL_FLAGS) -Wall

# Windows 95/98/ME (Win32, cross-compile from Linux)
w95: platform_win32.c game.c game.h
	$(CC_W95) -o progressbar95.exe platform_win32.c game.c \
		-lgdi32 -luser32 -mwindows -std=c89 -Wall

# Windows native (MinGW on Windows)
w95-native: platform_win32.c game.c game.h
	$(CC) -o progressbar95.exe platform_win32.c game.c \
		-lgdi32 -luser32 -mwindows -std=c89 -Wall

clean:
	rm -f progressbar95 progressbar95.exe
