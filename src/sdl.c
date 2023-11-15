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
 
 /*
 
 todo
 fix memory leak in menu
 fix ip input box default text
 add settings menu
 add servers history/quick connect
 */

#ifdef HAVE_SDL

#include "sdl.h"
#include "connection.h"
#include <Limelight.h>
#include "util.h"

SDLContext ctx;
SERVER_DATA server;
CONFIGURATION config;

static bool done;
static int fullscreen_flags;
SDL_mutex *mutex;
int sdlCurrentFrame, sdlNextFrame;
int eventPending = 0;
int pair_eval = 0;
const char **global_app_names = NULL;
int global_app_count = 0;

void sdl_base_ui(SDLContext *ctx) {
    SDL_FillRect(ctx->menu_surface, NULL, SDL_MapRGB(ctx->menu_surface->format, 0, 0, 0));

    SDL_Surface* png_surface = IMG_Load(TOP_BANNER);
    if (!png_surface) {
        fprintf(stderr, "Could not load PNG image: %s\n", IMG_GetError());
    } else {
        int x_position = (ctx->menu_surface->w - png_surface->w) / 2;

        SDL_Rect dest_rect;
        dest_rect.x = x_position;
        dest_rect.y = 0;
        dest_rect.w = png_surface->w;
        dest_rect.h = png_surface->h;

        SDL_BlitSurface(png_surface, NULL, ctx->menu_surface, &dest_rect);
        SDL_FreeSurface(png_surface);
    }
}

void sdl_init(SDLContext *ctx, int width, int height, bool fullscreen) {
    ctx->sdlCurrentFrame = ctx->sdlNextFrame = 0;

    if(SDL_Init(SDL_INIT_VIDEO | SDL_INIT_EVENTS)) {
        fprintf(stderr, "Could not initialize SDL - %s\n", SDL_GetError());
        exit(1);
    }

    Uint32 fullscreen_flags = fullscreen ? SDL_WINDOW_FULLSCREEN : 0;
    ctx->window = SDL_CreateWindow("Moonlight", SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED, width, height, SDL_WINDOW_OPENGL | fullscreen_flags);

    if(!ctx->window) {
        fprintf(stderr, "SDL: could not create window - exiting\n");
        exit(1);
    }

    ctx->renderer = SDL_CreateRenderer(ctx->window, -1, SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
    if (!ctx->renderer) {
    printf("SDL_CreateRenderer failed: %s\n", SDL_GetError());
    exit(1);
    }

    ctx->bmp = SDL_CreateTexture(ctx->renderer, SDL_PIXELFORMAT_YV12, SDL_TEXTUREACCESS_STREAMING, width, height);
    if (!ctx->bmp) {
    fprintf(stderr, "SDL: could not create texture - exiting\n");
    exit(1);
    }

    mutex = SDL_CreateMutex();
    if (!mutex) {
    fprintf(stderr, "Couldn't create mutex\n");
    exit(1);
    }

    SDL_RendererInfo info;
    if (SDL_GetRendererInfo(ctx->renderer, &info) == 0) {
        printf("Renderer Name: %s\n", info.name);
        for (Uint32 i = 0; i < info.num_texture_formats; i++) {
            printf("Texture Format %d: %s\n", i, SDL_GetPixelFormatName(info.texture_formats[i]));
        }
    }
    
    ctx->menu_surface = SDL_CreateRGBSurface(0, 640, 480, 16, 0xF800, 0x07E0, 0x001F, 0x0000);
    ctx->menu_texture = SDL_CreateTexture(ctx->renderer, SDL_PIXELFORMAT_RGB565, SDL_TEXTUREACCESS_STREAMING, 640, 480);
    
    if(TTF_Init()) {
        fprintf(stderr, "Could not initialize TTF - %s\n", SDL_GetError());
        exit(1);
    }
    
    launchConnectRemoteThread(&server, &config, ctx);
    sdl_base_ui(ctx);
    sdl_menu(ctx);  
}

void cleanupSDLContext(SDLContext *ctx) {
    if (ctx) {
        if (ctx->window) {
            SDL_DestroyWindow(ctx->window);
        }
        if (ctx->renderer) {
            SDL_DestroyRenderer(ctx->renderer);
        }
        if (ctx->bmp) {
            SDL_DestroyTexture(ctx->bmp);
        }
        if (ctx->mutex) {
            SDL_DestroyMutex(ctx->mutex);
        }
        if (ctx->menu_surface) {
            SDL_FreeSurface(ctx->menu_surface);
        }
        if (ctx->menu_texture) {
            SDL_DestroyTexture(ctx->menu_texture);
        }
        if (ctx->font) {
            TTF_CloseFont(ctx->font);
        }
        memset(ctx, 0, sizeof(SDLContext));
        config_save(MOONLIGHT_CONF, &config); 
        TTF_Quit();
        SDL_Quit();
        exit(-1); 
    }
}


void sdl_ip_input(SDLContext *ctx, SDL_Rect text_box_rect) {
    if (ctx == NULL) {
        fprintf(stderr, "Null context\n");
        return;
    }

    TTF_Font *font = TTF_OpenFont(MOONLIGHT_FONT, 30);
    if (!font) {
        fprintf(stderr, "TTF_OpenFont failed: %s\n", TTF_GetError());
        return;
    }

    const char *text_to_render = ctx->state.entered_ip;
    text_to_render = "Enter IP";

    SDL_Color text_color = {255, 255, 255, 0};
    SDL_Surface *text_surface = TTF_RenderText_Blended(font, text_to_render, text_color);
    
    if (!text_surface) {
        fprintf(stderr, "TTF_RenderText_Blended failed: %s\n", TTF_GetError());
        TTF_CloseFont(font);
        return;
    }

    SDL_Rect text_rect;
    text_rect.x = text_box_rect.x + (text_box_rect.w - text_surface->w) / 2;
    text_rect.y = text_box_rect.y + (text_box_rect.h - text_surface->h) / 2;
    text_rect.w = text_surface->w;
    text_rect.h = text_surface->h;
    
    SDL_Texture *text_texture = SDL_CreateTextureFromSurface(ctx->renderer, text_surface);
    SDL_RenderCopy(ctx->renderer, text_texture, NULL, &text_rect);

    SDL_FreeSurface(text_surface);
    SDL_DestroyTexture(text_texture);
    TTF_CloseFont(font);
}

void sdl_ip_input_gui(SDLContext *ctx) {
    sdl_base_ui(ctx);

    int text_box_width = 200;
    int text_box_height = 40;
    int text_box_x = (ctx->menu_surface->w - text_box_width) / 2;
    int text_box_y = (ctx->menu_surface->h - text_box_height) / 2;
    
    SDL_Rect text_box_rect;
    text_box_rect.x = text_box_x;
    text_box_rect.y = text_box_y;
    text_box_rect.w = text_box_width;
    text_box_rect.h = text_box_height;

    SDL_SetRenderDrawColor(ctx->renderer, 255, 255, 255, 255);
    SDL_RenderDrawRect(ctx->renderer, &text_box_rect);

    sdl_ip_input(ctx, text_box_rect);
}


void sdl_banner(SDLContext *ctx, const char *format, const char *color, ...) {
    // printf("Entered banner\n");
    if (!ctx || !ctx->menu_surface) return;
    
    char message[256];
    va_list args;
    
    va_start(args, color);
    vsnprintf(message, sizeof(message), format, args);
    va_end(args);
    
    SDL_Rect rect;
    rect.x = 0;
    rect.y = 0;
    rect.w = 640;
    rect.h = 40;

    Uint32 bannerColor;

    if (strcmp(color, "green") == 0) {
        bannerColor = SDL_MapRGB(ctx->menu_surface->format, 76, 125, 76);
    } else if (strcmp(color, "orange") == 0) {
        bannerColor = SDL_MapRGB(ctx->menu_surface->format, 255, 182, 83);
    } else {
        bannerColor = SDL_MapRGB(ctx->menu_surface->format, 255, 105, 97);
    }

    SDL_FillRect(ctx->menu_surface, &rect, bannerColor);

    if (message[0] != '\0') {
        TTF_Font *font = TTF_OpenFont(MOONLIGHT_FONT, 26);
        if (font == NULL) {
            fprintf(stderr, "Could not load font: %s\n", TTF_GetError());
            return;
        }

        SDL_Color textColor = {255, 255, 255, 255};
        SDL_Surface *textSurface = TTF_RenderText_Blended(font, message, textColor);

        if(!textSurface) {
            fprintf(stderr, "Could not render text: %s\n", TTF_GetError());
            TTF_CloseFont(font);
            return;
        }

        SDL_Rect textRect;
        textRect.x = (rect.w - textSurface->w) / 2;
        textRect.y = rect.y + (rect.h - textSurface->h) / 2;
        textRect.w = textSurface->w;
        textRect.h = textSurface->h;

        SDL_BlitSurface(textSurface, NULL, ctx->menu_surface, &textRect);

        SDL_FreeSurface(textSurface);
        TTF_CloseFont(font);
    }

    SDL_Texture* menu_texture = SDL_CreateTextureFromSurface(ctx->renderer, ctx->menu_surface);
    SDL_RenderCopy(ctx->renderer, menu_texture, NULL, NULL);
    SDL_DestroyTexture(menu_texture);
    SDL_RenderPresent(ctx->renderer);
    
    ctx->state.redrawAll = 0;
    eventPending = 0;
    // printf("Exit banner\n");
}

void sdl_draw_textbox(SDLContext *ctx, SDL_Surface* surface, const char* message) {
    int screen_width = 640;
    int screen_height = 480;
    int padding = 20;
    int box_height = 50;
    
    const char* displayMessage = message ? message : " ";

    int box_start_x = padding;
    int box_start_y = screen_height - box_height - padding - 210; 

    SDL_Rect box_rect;
    box_rect.x = box_start_x;
    box_rect.y = box_start_y;
    box_rect.w = screen_width - 2 * padding;
    box_rect.h = box_height;
    Uint32 box_color = SDL_MapRGB(surface->format, 211, 211, 211);
    SDL_FillRect(surface, &box_rect, box_color);

    if (TTF_Init() == -1) {
        fprintf(stderr, "Could not initialize SDL_ttf: %s\n", TTF_GetError());
        return;
    }
    TTF_Font *font = TTF_OpenFont(MOONLIGHT_FONT, 30);
    if (font == NULL) {
        fprintf(stderr, "Could not load font: %s\n", TTF_GetError());
        TTF_Quit();
        return;
    }

    SDL_Color textColor = {0, 0, 0, 0};
    SDL_Surface* textSurface = TTF_RenderText_Blended(font, displayMessage, textColor);
    SDL_Rect textRect;
    textRect.x = box_start_x + (box_rect.w - textSurface->w) / 2;
    textRect.y = box_start_y + (box_rect.h - textSurface->h) / 2;
    textRect.w = textSurface->w;
    textRect.h = textSurface->h;

    SDL_BlitSurface(textSurface, NULL, surface, &textRect);

    SDL_FreeSurface(textSurface);
    TTF_CloseFont(font);
    TTF_Quit();
}

void sdl_tile(SDLContext *ctx, SDL_Surface* surface, int columns, int rows, int selected, int index, const char **labels, int numItems, int font_size, int isAppSelection) {
    int screen_width = 640;
    int screen_height = 480;
    int padding = 20;
    int tile_gap = 20;
    
    int tile_height, tile_width, additional_offset, startX, startY;
    if (isAppSelection) {
        if (numItems <= 9) {
            columns = 3;
            tile_height = 120;
            additional_offset = 180;
        } else {
            columns = 6;
            tile_height = 60;
            additional_offset = 100;
        }
        ctx->state.currentColumns = columns;
        
        tile_width = (screen_width - 2 * padding - (tile_gap * (columns - 1))) / columns;
        if (numItems <= 9) {
            tile_width -= 10;
        }
        
    } else {
        tile_height = (numItems > 10) ? 50 : 100;
        tile_width = (screen_width - 2 * padding - (tile_gap * (columns - 1))) / columns;
        additional_offset = (numItems > 10) ? -135 : 0;
    }

    int total_tile_height = (tile_height * rows) + (tile_gap * (rows - 1));
    startX = padding;
    startY = screen_height - total_tile_height - padding - additional_offset;

    int x = index % columns;
    int y = index / columns;
        
    SDL_Rect rect;
    rect.x = startX + x * (tile_width + tile_gap);
    rect.y = startY + y * (tile_height + tile_gap);
    rect.w = tile_width;
    rect.h = tile_height;
    Uint32 bg_color = (selected == index) ? SDL_MapRGB(surface->format, 144, 144, 144) : SDL_MapRGB(surface->format, 108, 108, 108);
    SDL_FillRect(surface, &rect, bg_color);

    SDL_Rect bandRectBottom;
    bandRectBottom.x = rect.x;
    bandRectBottom.y = rect.y + rect.h - 10;
    bandRectBottom.w = rect.w;
    bandRectBottom.h = 10;
    Uint32 band_color_bottom = (selected == index) ? SDL_MapRGB(surface->format, 225, 0, 0) : SDL_MapRGB(surface->format, 98, 98, 98);
    SDL_FillRect(surface, &bandRectBottom, band_color_bottom);

    SDL_Rect bandRectTop;
    bandRectTop.x = rect.x;
    bandRectTop.y = rect.y;
    bandRectTop.w = rect.w;
    bandRectTop.h = 5;
    Uint32 band_color_top = (selected == index) ? SDL_MapRGB(surface->format, 164, 164, 164) : SDL_MapRGB(surface->format, 138, 138, 138);
    SDL_FillRect(surface, &bandRectTop, band_color_top);

      if (TTF_Init() == -1) {
        fprintf(stderr, "Could not initialize SDL_ttf: %s\n", TTF_GetError());
        return;
    }
    
    TTF_Font *font = TTF_OpenFont(MOONLIGHT_FONT, font_size);
    if (font == NULL) {
        fprintf(stderr, "Could not load font: %s\n", TTF_GetError());
        TTF_Quit();
        return;
    }

    SDL_Color textColor = {255, 255, 255, 0};

    SDL_Surface* textSurface = TTF_RenderText_Blended(font, labels[index], textColor);
    
    SDL_Rect textRect;
    textRect.x = rect.x + (rect.w - textSurface->w) / 2;
    textRect.y = rect.y + (rect.h - textSurface->h) / 2;
    textRect.w = textSurface->w;
    textRect.h = textSurface->h;

    SDL_BlitSurface(textSurface, NULL, surface, &textRect);
    
    SDL_FreeSurface(textSurface);
    TTF_CloseFont(font);
    TTF_Quit();
}

void handle_ip_input_space(SDL_Event *event, SDLContext *ctx, int *selected_item);
void handle_app_menu_input(SDL_Event *event, SDLContext *ctx, int *selected_item);
void handle_settings_input(SDL_Event *event, SDLContext *ctx, int *selected_item);
void handle_main_menu_input(SDL_Event *event, SDLContext *ctx, int *selected_item, const char *menu_texts[]);
void handle_backspace_input(SDLContext *ctx);
void handlePairClient(SDLContext *ctx);

void handle_key_input(SDL_Event *event, SDLContext *ctx, int *selected_item, const char *menu_texts[],
                      const char *settings_texts[], const char *ip_input[]) {
    eventPending = 1;
    if (ctx->state.inIPInput) {
        switch (event->key.keysym.sym) {
            case SDLK_SPACE:
                handle_ip_input_space(event, ctx, selected_item);
                break;
            case SDLK_BACKSPACE:
                *selected_item = 0;
                ctx->state.inIPInput = 0;
                ctx->state.redrawAll = 1;
                break;
            case SDLK_UP:
                *selected_item = (*selected_item - BIG_ROW + MAX_IP_TILES) % MAX_IP_TILES;
                ctx->state.redrawAll = 1;
                break;
            case SDLK_DOWN:
                *selected_item = (*selected_item + BIG_ROW) % MAX_IP_TILES;
                ctx->state.redrawAll = 1;
                break;
            case SDLK_LEFT:
                *selected_item = (*selected_item - 1 + MAX_IP_TILES) % MAX_IP_TILES;
                ctx->state.redrawAll = 1;
                break;
            case SDLK_RIGHT:
                *selected_item = (*selected_item + 1) % MAX_IP_TILES;
                ctx->state.redrawAll = 1;
                break;
            default:
                break;
        }
    } else if (ctx->state.inAppMenu) {
        handle_app_menu_input(event, ctx, selected_item);
    } else if (ctx->state.inSettings) {
        handle_settings_input(event, ctx, selected_item);
    } else {
        handle_main_menu_input(event, ctx, selected_item, menu_texts);
    }
}

void handle_ip_input_space(SDL_Event *event, SDLContext *ctx, int *selected_item) {
    switch (*selected_item) {
        case 0 ... 9: {
            char new_ip[64];
            snprintf(new_ip, sizeof(new_ip), "%s%d", ctx->state.entered_ip, *selected_item);
            ctx->state.entered_ip = strdup(new_ip);
            ctx->state.redrawAll = 1;
        }
            break;
        case 10: {
            char new_ip[64];
            snprintf(new_ip, sizeof(new_ip), "%s.", ctx->state.entered_ip);
            ctx->state.entered_ip = strdup(new_ip);
            ctx->state.redrawAll = 1;
        }
            break;
        case 11:
            ctx->state.inIPInput = 0;
            ctx->state.inSettings = 0;
            ctx->state.redrawAll = 1;
            break;
        case 12:
            handle_backspace_input(ctx);
            break;
        case 13:
            ctx->state.entered_ip = strdup(" ");
            ctx->state.redrawAll = 1;
            break;
        case 14:
            handlePairClient(ctx);
            break;
        default:
            // Nothing yet
            break;
    }
}

void handle_redraw(SDLContext *ctx, int *selected_item, const char *menu_texts[], const char *settings_texts[],
    const char *ip_input[]) {
    if (ctx->state.redrawAll) {
        sdl_base_ui(ctx);
        ctx->state.redrawAll = 0;

        if (!ctx->state.inSettings && !ctx->state.inIPInput && !ctx->state.inAppMenu) {
            for (int i = 0; i < 6; ++i) {
                sdl_tile(ctx, ctx->menu_surface, COLUMNS, ROWS, *selected_item, i, menu_texts, 6, 24, 0);
            }
        } else if (ctx->state.inSettings && !ctx->state.inIPInput && !ctx->state.inAppMenu) {
            for (int i = 0; i < 6; ++i) {
                sdl_tile(ctx, ctx->menu_surface, COLUMNS, ROWS, *selected_item, i, settings_texts, 6, 24, 0);
            }
        } else if (!ctx->state.inSettings && ctx->state.inIPInput && !ctx->state.inAppMenu) {
            for (int i = 0; i < 15; ++i) {
                sdl_tile(ctx, ctx->menu_surface, BIG_COL, BIG_ROW, *selected_item, i, ip_input, 15, 24, 0);
                sdl_draw_textbox(ctx, ctx->menu_surface, ctx->state.entered_ip);
            }
        } if (!ctx->state.inSettings && !ctx->state.inIPInput && ctx->state.inAppMenu) {
                const char **app_select = NULL;
                int count = applist(&server, &app_select);
                global_app_names = app_select;
                global_app_count = count;

                if (count > 0) {
                    for (int i = 0; i < count; ++i) {
                        sdl_tile(ctx, ctx->menu_surface, COLUMNS, ROWS, *selected_item, i, app_select, count, 16, 1);
                    }
                }
            }

        SDL_UpdateTexture(ctx->menu_texture, NULL, ctx->menu_surface->pixels, ctx->menu_surface->pitch);
        SDL_RenderCopy(ctx->renderer, ctx->menu_texture, NULL, NULL);
        SDL_RenderPresent(ctx->renderer);

        ctx->state.redrawAll = 0;
        eventPending = 0;
    }
}

void handle_app_menu_input(SDL_Event *event, SDLContext *ctx, int *selected_item) {
    switch (event->key.keysym.sym) {
        case SDLK_SPACE:
            if (*selected_item < global_app_count) {
                strcpy(config.app, global_app_names[*selected_item]);
                printf("Selected app: %s\n", global_app_names[*selected_item]);
                connectRemote(&server, &config, ctx);
                sdl_banner(ctx, "Host: %s App: %s", "orange", config.address, global_app_names[*selected_item]);
                handleStreaming(&server, &config);
            }
            break;
        case SDLK_BACKSPACE:
            ctx->state.inAppMenu = 0;
            ctx->state.redrawAll = 1;
            *selected_item = 0;
            break;
        case SDLK_ESCAPE:
            ctx->state.inAppMenu = 0;
            ctx->state.redrawAll = 1;
            break;
        case SDLK_UP:
            if (*selected_item - ctx->state.currentColumns >= 0) {
                *selected_item -= ctx->state.currentColumns;
                ctx->state.redrawAll = 1;
            }
            break;
        case SDLK_DOWN:
            if (*selected_item + ctx->state.currentColumns < global_app_count) {
                *selected_item += ctx->state.currentColumns;
                ctx->state.redrawAll = 1;
            }
            break;
        case SDLK_LEFT:
            if (*selected_item > 0) {
                *selected_item -= 1;
                ctx->state.redrawAll = 1;
            }
            break;
        case SDLK_RIGHT:
            if (*selected_item < global_app_count - 1) {
                *selected_item += 1;
                ctx->state.redrawAll = 1;
            }
            break;
        default:
            break;
    }
}

void handle_settings_input(SDL_Event *event, SDLContext *ctx, int *selected_item) {
    switch (event->key.keysym.sym) {
        case SDLK_SPACE:
            switch (*selected_item) {
                case 0:
                    sdl_banner(ctx, "Not yet", "red");
                    break;
                case 1:
                    sdl_banner(ctx, "Not yet", "red");
                    break;
                case 2:
                    sdl_banner(ctx, "Not yet", "red");
                    break;
                case 3:
                    sdl_banner(ctx, "Not yet", "red");
                    break;
                case 4:
                    sdl_banner(ctx, "Not yet", "red");
                    break;
                case 5:
                    ctx->state.inSettings = 0;
                    *selected_item = 0;
                    ctx->state.redrawAll = 1;
                default:
                    break;
            }
            break;
        case SDLK_BACKSPACE:
            ctx->state.inSettings = 0;
            ctx->state.redrawAll = 1;
            *selected_item = 0;
            break;
        case SDLK_ESCAPE:
            ctx->state.inSettings = 0;
            ctx->state.redrawAll = 1;
            *selected_item = 0;
            break;
        case SDLK_UP:
            *selected_item = (*selected_item - COLUMNS + MAX_ITEMS) % MAX_ITEMS;
            ctx->state.redrawAll = 1;
            break;
        case SDLK_DOWN:
            *selected_item = (*selected_item + COLUMNS) % MAX_ITEMS;
            ctx->state.redrawAll = 1;
            break;
        case SDLK_LEFT:
            *selected_item = (*selected_item - 1 + MAX_ITEMS) % MAX_ITEMS;
            ctx->state.redrawAll = 1;
            break;
        case SDLK_RIGHT:
            *selected_item = (*selected_item + 1) % MAX_ITEMS;
            ctx->state.redrawAll = 1;
            break;
        default:
            break;
    }
}

void handle_main_menu_input(SDL_Event *event, SDLContext *ctx, int *selected_item, const char *menu_texts[]) {
    switch (event->key.keysym.sym) {
        case SDLK_SPACE:
            switch (*selected_item) {
                case 0:
                    pair_eval = pair_check(&server);
                    if (pair_eval == 1) {
                        sdl_banner(ctx, "You must pair first!", "red");
                    } else {
                        ctx->state.redrawAll = 1;
                        *selected_item = 0;
                        ctx->state.inAppMenu = 1;
                        ctx->state.inSettings = 0;
                        ctx->state.inIPInput = 0;
                    }
                    break;
                case 1:
                    *selected_item = 0;
                    ctx->state.inIPInput = 1;
                    ctx->state.inSettings = 0;
                    ctx->state.redrawAll = 1;
                    break;
                case 2:
                    sdl_banner(ctx, "Please wait.... Unpairing", "orange");
                    unPairClient(ctx);
                    break;
                case 3:
                    sdl_banner(ctx, "Not implemented yet", "orange");
                    break;
                case 4:
                    sdl_banner(ctx, "Not implemented yet", "orange");
                    break;
                case 5:
                    ctx->state.exitNow = 1;
                default:
                    break;
            }
            break;
        case SDLK_BACKSPACE:
            ctx->state.exitNow = 1;
            break;
        case SDLK_ESCAPE:
            printf("Pressed ESCAPE\n");
            break;
        case SDLK_UP:
            *selected_item = (*selected_item - COLUMNS + MAX_ITEMS) % MAX_ITEMS;
            ctx->state.redrawAll = 1;
            break;
        case SDLK_DOWN:
            *selected_item = (*selected_item + COLUMNS) % MAX_ITEMS;
            ctx->state.redrawAll = 1;
            break;
        case SDLK_LEFT:
            *selected_item = (*selected_item - 1 + MAX_ITEMS) % MAX_ITEMS;
            ctx->state.redrawAll = 1;
            break;
        case SDLK_RIGHT:
            *selected_item = (*selected_item + 1) % MAX_ITEMS;
            ctx->state.redrawAll = 1;
            break;
        default:
            break;
    }
}

void handle_backspace_input(SDLContext *ctx) {
    if (ctx->state.entered_ip && strlen(ctx->state.entered_ip) > 0) {
        char new_ip[64];
        if (strlen(ctx->state.entered_ip) == 1) {
            new_ip[0] = ' ';
            new_ip[1] = '\0';
        } else {
            strncpy(new_ip, ctx->state.entered_ip, strlen(ctx->state.entered_ip) - 1);
            new_ip[strlen(ctx->state.entered_ip) - 1] = '\0';
        }
        free((void *) ctx->state.entered_ip);
        ctx->state.entered_ip = strdup(new_ip);
        ctx->state.redrawAll = 1;
    }
}

void handlePairClient(SDLContext *ctx) {
    if (ctx->state.entered_ip != NULL) {
        config.address = strdup(ctx->state.entered_ip);
        if (config.address == NULL) {
            fprintf(stderr, "Memory allocation failed for config->address\n");
        }
    } else {
        config.address = NULL;
    }

    if (config.address != NULL) {
        remove_spaces(config.address);
    }

    pairClient(ctx);
}

int sdl_menu(SDLContext *ctx) {
    SDL_Event event;

    ctx->state.inIPInput = 0;
    ctx->state.exitNow = 0;
    ctx->state.inSettings = 0;

    ctx->state.entered_ip = strdup(" ");
    int selected_item = 0;
    ctx->state.redrawAll = 1;
    eventPending = 1;

    char statusText[50] = "Status:";
    snprintf(statusText, sizeof(statusText), server.paired == 0 ? "Not paired!" : "Paired!");

    const char *menu_texts[6] = {"Stream", "Pair", "Unpair", "Settings", "Servers", "Exit"};
    const char *settings_texts[6] = {"Keybinds", "Mouse", " ", " ", " ", "Back"};
    const char *ip_input[15] = {"0", "1", "2", "3", "4", "5", "6", "7", "8", "9", ".", "Exit", "Del", "Clear", "Enter"};

    while (1) {
        while (SDL_PollEvent(&event)) {
            if (event.type == SDL_QUIT) {
                return 1;
            }
            if (event.type == SDL_KEYDOWN) {
                eventPending = 1;
                handle_key_input(&event, ctx, &selected_item, menu_texts, settings_texts, ip_input);
            }
        }

        if (eventPending) {
            handle_redraw(ctx, &selected_item, menu_texts, settings_texts, ip_input);
            if (ctx->state.exitNow) {
                break;
            }
        }

        SDL_Delay(25);
    }

    return 0;
}

void sdl_loop(SDLContext *ctx) {
  SDL_Event event;
  SDL_SetRelativeMouseMode(SDL_TRUE);
  done = 0;
  // printf("Entered Loop\n");

  while(!done && SDL_WaitEvent(&event)) {
    if (ctx->state.exitNow == 1) {
        done = true;
        break;
    }

    switch (sdlinput_handle_event(ctx->window, &event)) {
    case SDL_QUIT_APPLICATION:
      done = true;
      break;
    case SDL_TOGGLE_FULLSCREEN:
      fullscreen_flags ^= SDL_WINDOW_FULLSCREEN;
      SDL_SetWindowFullscreen(ctx->window, fullscreen_flags);
      break;
    case SDL_MOUSE_GRAB:
      SDL_ShowCursor(SDL_ENABLE);
      SDL_SetRelativeMouseMode(SDL_TRUE);
      break;
    case SDL_MOUSE_UNGRAB:
      SDL_SetRelativeMouseMode(SDL_FALSE);
      SDL_ShowCursor(SDL_DISABLE);
      break;
    default:
      if (event.type == SDL_QUIT)
        done = true;
      else if (event.type == SDL_USEREVENT) {
        if (event.user.code == SDL_CODE_FRAME) {
          if (++sdlCurrentFrame <= sdlNextFrame - SDL_BUFFER_FRAMES) {
            //Skip frame
          } else if (SDL_LockMutex(mutex) == 0) {
            Uint8** data = ((Uint8**) event.user.data1);
            int* linesize = ((int*) event.user.data2);
            SDL_UpdateYUVTexture(ctx->bmp, NULL, data[0], linesize[0], data[1], linesize[1], data[2], linesize[2]);
            SDL_UnlockMutex(mutex);
            SDL_RenderClear(ctx->renderer);
            SDL_RenderCopy(ctx->renderer, ctx->bmp, NULL, NULL);
            SDL_RenderPresent(ctx->renderer);
          } else
            fprintf(stderr, "Couldn't lock mutex\n");
        }
      }
    }
  }

// quitRemote(&server, &config, ctx);
printf("EXIT LOOP\n");
cleanupSDLContext(ctx);
}


void sdl_splash(SDLContext *ctx) { // not used currently

    SDL_Event event;

    Uint32 rmask = 0xF800;
    Uint32 gmask = 0x07E0;
    Uint32 bmask = 0x001F;
    Uint32 amask = 0x0000;

    SDL_Surface* menu_surface = SDL_CreateRGBSurface(0, 640, 480, 16, rmask, gmask, bmask, amask);
    if (!menu_surface) {
        fprintf(stderr, "SDL: could not create surface - %s\n", SDL_GetError());
        exit(1);
    }
    
    SDL_FillRect(menu_surface, NULL, SDL_MapRGB(menu_surface->format, 64, 64, 64));
    SDL_Texture* menu_texture = NULL;

    const char* image_path = SPLASH_PATH;
    int nextFrameNumber = 0;

    while (1) {
        char filename[256];
        snprintf(filename, sizeof(filename), "%s/frame%04d.png", image_path, nextFrameNumber);
        SDL_Surface* png_surface = IMG_Load(filename);

        if (!png_surface) {
            fprintf(stderr, "Could not load PNG image: %s\n", IMG_GetError());
            break;
        }

        SDL_BlitSurface(png_surface, NULL, menu_surface, NULL);
        SDL_FreeSurface(png_surface);

        if (!menu_texture) {
            menu_texture = SDL_CreateTextureFromSurface(ctx->renderer, menu_surface);
        } else {
            SDL_UpdateTexture(menu_texture, NULL, menu_surface->pixels, menu_surface->pitch);
        }

        nextFrameNumber++;

        while (SDL_PollEvent(&event)) {
            if (event.type == SDL_QUIT) {
                printf("Received SDL_QUIT event\n");
                goto cleanup;
            }
            if (event.type == SDL_KEYDOWN) {
                switch (event.key.keysym.sym) {
                    case SDLK_a:
                        printf("Pressed A\n");
                        break;
                    case SDLK_b:
                        printf("Pressed B\n");
                        break;
                    case SDLK_ESCAPE:
                        printf("Pressed ESCAPE\n");
                        goto cleanup;
                }
            }
        }

        if (menu_texture) {
            SDL_RenderCopy(ctx->renderer, menu_texture, NULL, NULL);
            SDL_RenderPresent(ctx->renderer);
        }

        SDL_Delay(30);
    }

cleanup:
    SDL_FreeSurface(menu_surface);
    if (menu_texture) {
        SDL_DestroyTexture(menu_texture);
    }
    printf("Destroyed menu_texture\n");
}

#endif /* HAVE_SDL */

