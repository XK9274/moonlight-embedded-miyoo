#ifndef MOONLIGHT_SDL_HEADER_H
#define MOONLIGHT_SDL_HEADER_H

/*
 * This file is part of Moonlight Embedded.
 *
 * Copyright (C) 2015 Iwan Timmer
 *
 * Moonlight is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 3 of the License, or
 * (at your option) any later version.
 *
 * Moonlight is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with Moonlight; if not, see <http://www.gnu.org/licenses/>.
 */

#ifdef HAVE_SDL

#include <SDL.h>
#include <SDL_image.h>
#include <SDL_ttf.h>

#include <stdbool.h>

#define SDL_NOTHING 0
#define SDL_QUIT_APPLICATION 1
#define SDL_MOUSE_GRAB 2
#define SDL_MOUSE_UNGRAB 3
#define SDL_TOGGLE_FULLSCREEN 4

#define SDL_CODE_FRAME 0

#define SDL_BUFFER_FRAMES 2

#define SPLASH_PATH "/mnt/SDCARD/App/moonlight/res/splash"
#define MIYOO_VERSION "1.3"
#define TOP_BANNER "/mnt/SDCARD/App/moonlight/res/icon/top_banner.png"
#define MOONLIGHT_FONT "/mnt/SDCARD/miyoo/app/Helvetica-Neue-2.ttf"

#define MAX_ITEMS 6
#define ROWS 2
#define COLUMNS 3

#define BIG_COL 5
#define BIG_ROW 5
#define MAX_IP_TILES 15

typedef struct {
    int redraw;
    int redrawAll;
    int inSettings;
    int inIPInput;
    int noPairStart;
    int unPairedNoti;
    const char* entered_ip;
    char received_pin[5];
} UIState;

typedef struct SDLContext {
    SDL_Window *window;
    SDL_Renderer *renderer;
    SDL_Texture *bmp;
    SDL_mutex *mutex;
    SDL_Surface *menu_surface;
    SDL_Texture *menu_texture;
    TTF_Font *font;
    int sdlCurrentFrame, sdlNextFrame;
    bool fullscreen;
    UIState state;
} SDLContext;

void sdl_init(SDLContext *ctx, int width, int height, bool fullscreen);
void sdl_banner(SDLContext *ctx, const char *text, const char *color);
void sdl_tile(SDLContext *ctx, SDL_Surface* surface, int columns, int rows, int selected, int index, const char* text, int numItems);
int sdl_menu(SDLContext *ctx);
void sdl_unpair(SDLContext *ctx, const char *IPADDR);
void sdl_loop(SDLContext *ctx);
void sdl_splash(SDLContext *ctx);
void cleanupSDLContext(SDLContext *ctx);

extern SDL_mutex *mutex;
extern int sdlCurrentFrame, sdlNextFrame;

#endif /* HAVE_SDL */

#endif /* MOONLIGHT_SDL_HEADER_H */